#ifndef _DMABUF_MANAGER_H
#define _DMABUF_MANAGER_H

#include <stdint.h>
#include <pthread.h>

#define DMA_BUF_FREE    0
#define DMA_BUF_WRITING 1
#define DMA_BUF_READY   2
#define DMA_BUF_READING 3

typedef struct {
    int fd;
    int state;
    int64_t frame_id;
} T_DmaBuf;

typedef struct {
    T_DmaBuf *bufs;
    int count;
    int size;
    const char *name;
    volatile int *exit_flag;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} T_DmaBufPool;

int DmaHeapAlloc(int size);
int DmaBufPoolInit(T_DmaBufPool *pool,
                   int count,
                   int size,
                   const char *name,
                   volatile int *exit_flag);
void DmaBufPoolDeinit(T_DmaBufPool *pool);
T_DmaBuf *DmaBufPoolGetBuf(T_DmaBufPool *pool, int idx);
int DmaBufPoolGetWriteIndex(T_DmaBufPool *pool, int wait, int allow_overwrite_ready);
void DmaBufPoolPublish(T_DmaBufPool *pool,
                       int idx,
                       int64_t frame_id,
                       int drop_other_ready);
int DmaBufPoolGetReadIndex(T_DmaBufPool *pool);
void DmaBufPoolReleaseIndex(T_DmaBufPool *pool, int idx);
void DmaBufPoolWakeAll(T_DmaBufPool *pool);

#endif /* _DMABUF_MANAGER_H */
