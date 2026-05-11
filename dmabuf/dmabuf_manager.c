
#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dmabuf_manager.h"

#ifndef DMA_HEAP_IOC_MAGIC
#define DMA_HEAP_IOC_MAGIC 'H'
struct dma_heap_allocation_data {
    __u64 len;
    __u32 fd;
    __u32 fd_flags;
    __u64 heap_flags;
};
#define DMA_HEAP_IOCTL_ALLOC _IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)
#endif

int DmaHeapAlloc(int size)
{
    const char *heaps[] = {
        "/dev/dma_heap/cma",
        "/dev/dma_heap/system-uncached",
        "/dev/dma_heap/system",
    };
    struct dma_heap_allocation_data data;
    int heap_fd = -1;
    int dma_fd = -1;
    int page_size;
    int alloc_size;

    for (unsigned int i = 0; i < sizeof(heaps) / sizeof(heaps[0]); i++) {
        heap_fd = open(heaps[i], O_RDWR | O_CLOEXEC);
        if (heap_fd >= 0) {
            printf("use dma heap: %s\n", heaps[i]);
            break;
        }
    }

    if (heap_fd < 0) {
        perror("open dma_heap");
        return -1;
    }

    page_size = getpagesize();
    alloc_size = (size + page_size - 1) & ~(page_size - 1);

    memset(&data, 0, sizeof(data));
    data.len = alloc_size;
    data.fd_flags = O_RDWR | O_CLOEXEC;

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) == 0) {
        dma_fd = (int)data.fd;
        printf("dma heap alloc ok: fd=%d size=%d alloc_size=%d\n",
               dma_fd, size, alloc_size);
    } else {
        perror("DMA_HEAP_IOCTL_ALLOC");
    }

    close(heap_fd);
    return dma_fd;
}

int DmaBufPoolInit(T_DmaBufPool *pool,
                   int count,
                   int size,
                   const char *name,
                   volatile int *exit_flag)
{
    if (!pool || count <= 0 || size <= 0)
        return -1;

    memset(pool, 0, sizeof(*pool));
    pool->bufs = (T_DmaBuf *)calloc(count, sizeof(T_DmaBuf));
    if (!pool->bufs)
        return -1;

    pool->count = count;
    pool->size = size;
    pool->name = name ? name : "dmabuf";
    pool->exit_flag = exit_flag;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (int i = 0; i < count; i++)
        pool->bufs[i].fd = -1;

    for (int i = 0; i < count; i++) {
        pool->bufs[i].fd = DmaHeapAlloc(size);
        if (pool->bufs[i].fd < 0) {
            DmaBufPoolDeinit(pool);
            return -1;
        }

        pool->bufs[i].state = DMA_BUF_FREE;
        pool->bufs[i].frame_id = 0;
        printf("%s dma fd ready: idx=%d fd=%d size=%d\n",
               pool->name, i, pool->bufs[i].fd, size);
    }

    return 0;
}

void DmaBufPoolDeinit(T_DmaBufPool *pool)
{
    if (!pool)
        return;

    if (pool->bufs) {
        for (int i = 0; i < pool->count; i++) {
            if (pool->bufs[i].fd >= 0)
                close(pool->bufs[i].fd);
        }
        free(pool->bufs);
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    memset(pool, 0, sizeof(*pool));
}

T_DmaBuf *DmaBufPoolGetBuf(T_DmaBufPool *pool, int idx)
{
    if (!pool || !pool->bufs || idx < 0 || idx >= pool->count)
        return NULL;

    return &pool->bufs[idx];
}

int DmaBufPoolGetWriteIndex(T_DmaBufPool *pool, int wait, int allow_overwrite_ready)
{
    int idx = -1;

    if (!pool)
        return -1;

    pthread_mutex_lock(&pool->mutex);
    while (!pool->exit_flag || !*pool->exit_flag) {
        for (int i = 0; i < pool->count; i++) {
            if (pool->bufs[i].state == DMA_BUF_FREE) {
                idx = i;
                break;
            }
        }

        if (idx >= 0)
            break;

        if (allow_overwrite_ready) {
            for (int i = 0; i < pool->count; i++) {
                if (pool->bufs[i].state == DMA_BUF_READY) {
                    idx = i;
                    break;
                }
            }
        }

        if (idx >= 0 || !wait)
            break;

        pthread_cond_wait(&pool->cond, &pool->mutex);
    }

    if (idx >= 0)
        pool->bufs[idx].state = DMA_BUF_WRITING;

    pthread_mutex_unlock(&pool->mutex);
    return idx;
}

void DmaBufPoolPublish(T_DmaBufPool *pool,
                       int idx,
                       int64_t frame_id,
                       int drop_other_ready)
{
    if (!pool || idx < 0 || idx >= pool->count)
        return;

    pthread_mutex_lock(&pool->mutex);

    if (drop_other_ready) {
        for (int i = 0; i < pool->count; i++) {
            if (i != idx && pool->bufs[i].state == DMA_BUF_READY)
                pool->bufs[i].state = DMA_BUF_FREE;
        }
    }

    pool->bufs[idx].frame_id = frame_id;
    pool->bufs[idx].state = DMA_BUF_READY;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

int DmaBufPoolGetReadIndex(T_DmaBufPool *pool)
{
    int idx = -1;

    if (!pool)
        return -1;

    pthread_mutex_lock(&pool->mutex);
    while (!pool->exit_flag || !*pool->exit_flag) {
        for (int i = 0; i < pool->count; i++) {
            if (pool->bufs[i].state == DMA_BUF_READY) {
                idx = i;
                break;
            }
        }

        if (idx >= 0)
            break;

        pthread_cond_wait(&pool->cond, &pool->mutex);
    }

    if (idx >= 0)
        pool->bufs[idx].state = DMA_BUF_READING;

    pthread_mutex_unlock(&pool->mutex);
    return idx;
}

void DmaBufPoolReleaseIndex(T_DmaBufPool *pool, int idx)
{
    if (!pool || idx < 0 || idx >= pool->count)
        return;

    pthread_mutex_lock(&pool->mutex);
    pool->bufs[idx].state = DMA_BUF_FREE;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

void DmaBufPoolWakeAll(T_DmaBufPool *pool)
{
    if (!pool)
        return;

    pthread_mutex_lock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}
