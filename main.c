// #include <unistd.h>
// #include <stdlib.h>
// #include <stdio.h>
// #include <string.h>
// #include <pthread.h>
// #include <fcntl.h>
// #include <sys/ioctl.h>
// #include <sys/mman.h>
// #include <linux/types.h>
// #include <time.h>

// #include <config.h>
// #include <video_manager.h>
// #include <Ffmpeg_stream.h>
// #include <infer_bridge.h>

// #include "rga/im2d.h"
// #include "rga/rga.h"

// #define RGA_DMA_BUF_COUNT 2
// #define INFER_DMA_BUF_COUNT 2
// #define INFER_INPUT_WIDTH 224
// #define INFER_INPUT_HEIGHT 224
// #define INFER_INTERVAL_MS 500

// #ifndef DMA_HEAP_IOC_MAGIC
// #define DMA_HEAP_IOC_MAGIC 'H'
// struct dma_heap_allocation_data {
//     __u64 len;
//     __u32 fd;
//     __u32 fd_flags;
//     __u64 heap_flags;
// };
// #define DMA_HEAP_IOCTL_ALLOC _IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)
// #endif

// #define DMA_BUF_FREE    0
// #define DMA_BUF_WRITING 1
// #define DMA_BUF_READY   2
// #define DMA_BUF_READING 3

// typedef struct {
//     int fd;
//     int state;
//     int64_t frame_id;
// } T_DmaBuf;

// static int g_video_width;
// static int g_video_height;
// static int g_nv12_line_bytes;
// static int g_nv12_frame_size;
// static int g_infer_rgb_line_bytes;
// static int g_infer_rgb_frame_size;
// static pthread_t g_rga_thread;
// static pthread_t g_push_thread;
// #if INFER_ENABLE
// static pthread_t g_infer_thread;
// #endif
// static volatile int g_exit_flag = 0;

// static T_DmaBuf g_dma_bufs[RGA_DMA_BUF_COUNT];
// static pthread_mutex_t g_dma_mutex = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t g_dma_cond = PTHREAD_COND_INITIALIZER;

// #if INFER_ENABLE
// static T_DmaBuf g_infer_dma_bufs[INFER_DMA_BUF_COUNT];
// static pthread_mutex_t g_infer_dma_mutex = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t g_infer_dma_cond = PTHREAD_COND_INITIALIZER;
// #endif

// static T_VideoBuf g_rga_job;
// static int g_rga_job_pending = 0;
// static int g_rga_job_done = 0;
// static pthread_mutex_t g_rga_mutex = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t g_rga_job_cond = PTHREAD_COND_INITIALIZER;
// static pthread_cond_t g_rga_done_cond = PTHREAD_COND_INITIALIZER;

// static int g_rga_count = 0;
// static int g_push_count = 0;

// static long long GetMonoMs(void)
// {
//     struct timespec ts;

//     clock_gettime(CLOCK_MONOTONIC, &ts);
//     return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
// }

// static int ShouldRunInfer(void)
// {
//     static long long last_infer_ms = 0;
//     long long now_ms = GetMonoMs();

//     if (last_infer_ms == 0 || now_ms - last_infer_ms >= INFER_INTERVAL_MS) {
//         last_infer_ms = now_ms;
//         return 1;
//     }

//     return 0;
// }

// static int DmaHeapAlloc(int size)
// {
//     const char *heaps[] = {
//         "/dev/dma_heap/cma",
//         "/dev/dma_heap/system-uncached",
//         "/dev/dma_heap/system",
//     };
//     struct dma_heap_allocation_data data;
//     int heap_fd = -1;
//     int dma_fd = -1;
//     int page_size;
//     int alloc_size;

//     for (unsigned int i = 0; i < sizeof(heaps) / sizeof(heaps[0]); i++) {
//         heap_fd = open(heaps[i], O_RDWR | O_CLOEXEC);
//         if (heap_fd >= 0) {
//             printf("use dma heap: %s\n", heaps[i]);
//             break;
//         }
//     }

//     if (heap_fd < 0) {
//         perror("open dma_heap");
//         return -1;
//     }

//     page_size = getpagesize();
//     alloc_size = (size + page_size - 1) & ~(page_size - 1);

//     memset(&data, 0, sizeof(data));
//     data.len = alloc_size;
//     data.fd_flags = O_RDWR | O_CLOEXEC;

//     if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) == 0) {
//         dma_fd = (int)data.fd;
//         printf("dma heap alloc ok: fd=%d size=%d alloc_size=%d\n",
//                dma_fd, size, alloc_size);
//     } else {
//         perror("DMA_HEAP_IOCTL_ALLOC");
//     }

//     close(heap_fd);
//     return dma_fd;
// }

// static int DmaPoolInit(void)
// {
//     for (int i = 0; i < RGA_DMA_BUF_COUNT; i++) {
//         g_dma_bufs[i].fd = DmaHeapAlloc(g_nv12_frame_size);
//         if (g_dma_bufs[i].fd < 0)
//             return -1;

//         printf("RGA dma fd ready: idx=%d fd=%d size=%d\n",
//                i,
//                g_dma_bufs[i].fd,
//                g_nv12_frame_size);

//         g_dma_bufs[i].state = DMA_BUF_FREE;
//     }

//     return 0;
// }

// #if INFER_ENABLE
// static int InferDmaPoolInit(void)
// {
//     g_infer_rgb_line_bytes = INFER_INPUT_WIDTH * 3;
//     g_infer_rgb_frame_size = g_infer_rgb_line_bytes * INFER_INPUT_HEIGHT;

//     for (int i = 0; i < INFER_DMA_BUF_COUNT; i++) {
//         g_infer_dma_bufs[i].fd = DmaHeapAlloc(g_infer_rgb_frame_size);
//         if (g_infer_dma_bufs[i].fd < 0)
//             return -1;

//         g_infer_dma_bufs[i].state = DMA_BUF_FREE;
//         g_infer_dma_bufs[i].frame_id = 0;

//         printf("Infer RGB888 dma fd ready: idx=%d fd=%d %dx%d stride=%d size=%d\n",
//                i,
//                g_infer_dma_bufs[i].fd,
//                INFER_INPUT_WIDTH,
//                INFER_INPUT_HEIGHT,
//                g_infer_rgb_line_bytes,
//                g_infer_rgb_frame_size);
//     }

//     return 0;
// }

// static int InferPoolGetWriteIndexNoWait(void)
// {
//     int idx = -1;

//     pthread_mutex_lock(&g_infer_dma_mutex);

//     for (int i = 0; i < INFER_DMA_BUF_COUNT; i++) {
//         if (g_infer_dma_bufs[i].state == DMA_BUF_FREE) {
//             idx = i;
//             break;
//         }
//     }

//     if (idx < 0) {
//         for (int i = 0; i < INFER_DMA_BUF_COUNT; i++) {
//             if (g_infer_dma_bufs[i].state == DMA_BUF_READY) {
//                 idx = i;
//                 break;
//             }
//         }
//     }

//     if (idx >= 0)
//         g_infer_dma_bufs[idx].state = DMA_BUF_WRITING;

//     pthread_mutex_unlock(&g_infer_dma_mutex);
//     return idx;
// }

// static void InferPoolPublish(int idx, int64_t frame_id)
// {
//     pthread_mutex_lock(&g_infer_dma_mutex);

//     for (int i = 0; i < INFER_DMA_BUF_COUNT; i++) {
//         if (i != idx && g_infer_dma_bufs[i].state == DMA_BUF_READY)
//             g_infer_dma_bufs[i].state = DMA_BUF_FREE;
//     }

//     g_infer_dma_bufs[idx].frame_id = frame_id;
//     g_infer_dma_bufs[idx].state = DMA_BUF_READY;
//     pthread_cond_signal(&g_infer_dma_cond);
//     pthread_mutex_unlock(&g_infer_dma_mutex);
// }

// static int InferPoolGetReadIndex(void)
// {
//     int idx = -1;

//     pthread_mutex_lock(&g_infer_dma_mutex);
//     while (!g_exit_flag) {
//         for (int i = 0; i < INFER_DMA_BUF_COUNT; i++) {
//             if (g_infer_dma_bufs[i].state == DMA_BUF_READY) {
//                 idx = i;
//                 break;
//             }
//         }

//         if (idx >= 0)
//             break;

//         pthread_cond_wait(&g_infer_dma_cond, &g_infer_dma_mutex);
//     }

//     if (idx >= 0)
//         g_infer_dma_bufs[idx].state = DMA_BUF_READING;

//     pthread_mutex_unlock(&g_infer_dma_mutex);
//     return idx;
// }

// static void InferPoolReleaseIndex(int idx)
// {
//     pthread_mutex_lock(&g_infer_dma_mutex);
//     g_infer_dma_bufs[idx].state = DMA_BUF_FREE;
//     pthread_cond_signal(&g_infer_dma_cond);
//     pthread_mutex_unlock(&g_infer_dma_mutex);
// }
// #endif

// static int DmaPoolGetWriteIndex(void)
// {
//     int idx = -1;

//     pthread_mutex_lock(&g_dma_mutex);
//     while (!g_exit_flag) {
//         for (int i = 0; i < RGA_DMA_BUF_COUNT; i++) {
//             if (g_dma_bufs[i].state == DMA_BUF_FREE) {
//                 idx = i;
//                 break;
//             }
//         }

//         if (idx >= 0)
//             break;

//         for (int i = 0; i < RGA_DMA_BUF_COUNT; i++) {
//             if (g_dma_bufs[i].state == DMA_BUF_READY) {
//                 idx = i;
//                 break;
//             }
//         }

//         if (idx >= 0)
//             break;

//         pthread_cond_wait(&g_dma_cond, &g_dma_mutex);
//     }

//     if (idx >= 0)
//         g_dma_bufs[idx].state = DMA_BUF_WRITING;

//     pthread_mutex_unlock(&g_dma_mutex);
//     return idx;
// }

// static void DmaPoolPublish(int idx)
// {
//     pthread_mutex_lock(&g_dma_mutex);

//     for (int i = 0; i < RGA_DMA_BUF_COUNT; i++) {
//         if (i != idx && g_dma_bufs[i].state == DMA_BUF_READY)
//             g_dma_bufs[i].state = DMA_BUF_FREE;
//     }

//     g_dma_bufs[idx].state = DMA_BUF_READY;

//     if (g_rga_count <= 10)
//         printf("RGA publish dma idx=%d fd=%d\n", idx, g_dma_bufs[idx].fd);

//     pthread_cond_signal(&g_dma_cond);
//     pthread_mutex_unlock(&g_dma_mutex);
// }

// static int DmaPoolGetReadIndex(void)
// {
//     int idx = -1;

//     pthread_mutex_lock(&g_dma_mutex);
//     while (!g_exit_flag) {
//         for (int i = 0; i < RGA_DMA_BUF_COUNT; i++) {
//             if (g_dma_bufs[i].state == DMA_BUF_READY) {
//                 idx = i;
//                 break;
//             }
//         }

//         if (idx >= 0)
//             break;

//         pthread_cond_wait(&g_dma_cond, &g_dma_mutex);
//     }

//     if (idx >= 0)
//         g_dma_bufs[idx].state = DMA_BUF_READING;

//     pthread_mutex_unlock(&g_dma_mutex);
//     return idx;
// }

// static void DmaPoolReleaseReadIndex(int idx)
// {
//     pthread_mutex_lock(&g_dma_mutex);
//     g_dma_bufs[idx].state = DMA_BUF_FREE;
//     pthread_cond_signal(&g_dma_cond);
//     pthread_mutex_unlock(&g_dma_mutex);
// }

// static int RgaYuyvToNv12(void *src_buf, int dst_idx)
// {
//     static int rga_debug_count = 0;
//     rga_buffer_t src;
//     rga_buffer_t dst;
//     int ret;

//     src = wrapbuffer_virtualaddr(src_buf,
//                                  g_video_width,
//                                  g_video_height,
//                                  RK_FORMAT_YUYV_422);
//     dst = wrapbuffer_fd(g_dma_bufs[dst_idx].fd,
//                         g_video_width,
//                         g_video_height,
//                         RK_FORMAT_YCbCr_420_SP);

//     if (rga_debug_count < 10) {
//         printf("RGA convert begin src=%p dst_idx=%d dst_fd=%d %dx%d size=%d\n",
//                src_buf,
//                dst_idx,
//                g_dma_bufs[dst_idx].fd,
//                g_video_width,
//                g_video_height,
//                g_nv12_frame_size);
//         fflush(stdout);
//     }

//     ret = imcvtcolor(src, dst,
//                      RK_FORMAT_YUYV_422,
//                      RK_FORMAT_YCbCr_420_SP);

//     if (rga_debug_count < 10) {
//         printf("RGA convert ret=%d\n", ret);
//         fflush(stdout);
//         rga_debug_count++;
//     }

//     if (ret != IM_STATUS_SUCCESS) {
//         printf("RGA convert failed: %d\n", ret);
//         fflush(stdout);
//         return -1;
//     }

//     return 0;
// }

// #if INFER_ENABLE
// static int RgaYuyvToRgb888Infer(void *src_buf, int dst_idx)
// {
//     static int infer_debug_count = 0;
//     rga_buffer_t src;
//     rga_buffer_t dst;
//     int ret;

//     if (dst_idx < 0 || dst_idx >= INFER_DMA_BUF_COUNT)
//         return -1;

//     src = wrapbuffer_virtualaddr(src_buf,
//                                  g_video_width,
//                                  g_video_height,
//                                  RK_FORMAT_YUYV_422);
//     dst = wrapbuffer_fd(g_infer_dma_bufs[dst_idx].fd,
//                         INFER_INPUT_WIDTH,
//                         INFER_INPUT_HEIGHT,
//                         RK_FORMAT_RGB_888);

//     ret = imcvtcolor(src, dst,
//                      RK_FORMAT_YUYV_422,
//                      RK_FORMAT_RGB_888);

//     if (infer_debug_count < 10) {
//         printf("RGA infer convert ret=%d src=%dx%d dst_rgb=%dx%d fd=%d\n",
//                ret,
//                g_video_width,
//                g_video_height,
//                INFER_INPUT_WIDTH,
//                INFER_INPUT_HEIGHT,
//                g_infer_dma_bufs[dst_idx].fd);
//         fflush(stdout);
//         infer_debug_count++;
//     }

//     if (ret != IM_STATUS_SUCCESS) {
//         printf("RGA infer RGB888 convert failed: %d\n", ret);
//         fflush(stdout);
//         return -1;
//     }

//     return 0;
// }
// #endif

// static void* RgaThread(void* arg)
// {
//     T_VideoBuf raw_buf;
//     int dma_idx;
// #if INFER_ENABLE
//     int infer_idx;
// #endif

//     (void)arg;
//     memset(&raw_buf, 0, sizeof(raw_buf));

//     while (!g_exit_flag) {
//         pthread_mutex_lock(&g_rga_mutex);
//         while (!g_rga_job_pending && !g_exit_flag)
//             pthread_cond_wait(&g_rga_job_cond, &g_rga_mutex);

//         if (g_exit_flag) {
//             pthread_mutex_unlock(&g_rga_mutex);
//             break;
//         }

//         raw_buf = g_rga_job;
//         g_rga_job_pending = 0;
//         pthread_mutex_unlock(&g_rga_mutex);

//         dma_idx = DmaPoolGetWriteIndex();
//         if (dma_idx < 0)
//             break;

//         if (RgaYuyvToNv12(raw_buf.tPixelDatas.aucPixelDatas, dma_idx) == 0) {
//             g_rga_count++;
//             DmaPoolPublish(dma_idx);
// #if INFER_ENABLE
//             if (ShouldRunInfer()) {
//                 infer_idx = InferPoolGetWriteIndexNoWait();
//                 if (infer_idx >= 0) {
//                     if (RgaYuyvToRgb888Infer(raw_buf.tPixelDatas.aucPixelDatas,
//                                              infer_idx) == 0) {
//                         InferPoolPublish(infer_idx, g_rga_count);
//                     } else {
//                         InferPoolReleaseIndex(infer_idx);
//                     }
//                 }
//             }
// #endif
//         } else {
//             DmaPoolReleaseReadIndex(dma_idx);
//         }

//         pthread_mutex_lock(&g_rga_mutex);
//         g_rga_job_done = 1;
//         pthread_cond_signal(&g_rga_done_cond);
//         pthread_mutex_unlock(&g_rga_mutex);
//     }

//     return NULL;
// }

// #if INFER_ENABLE
// static void* InferThread(void* arg)
// {
//     int infer_idx;
//     int count = 0;

//     (void)arg;

//     while (!g_exit_flag) {
//         infer_idx = InferPoolGetReadIndex();
//         if (infer_idx < 0)
//             break;

//         count++;
//         if (count <= 10)
//             printf("InferThread got rgb dma idx=%d fd=%d frame=%lld\n",
//                    infer_idx,
//                    g_infer_dma_bufs[infer_idx].fd,
//                    (long long)g_infer_dma_bufs[infer_idx].frame_id);

//         InferBridgeFeedRgb888Dma(g_infer_dma_bufs[infer_idx].fd,
//                                  INFER_INPUT_WIDTH,
//                                  INFER_INPUT_HEIGHT,
//                                  g_infer_rgb_line_bytes,
//                                  g_infer_rgb_frame_size,
//                                  g_infer_dma_bufs[infer_idx].frame_id);

//         InferPoolReleaseIndex(infer_idx);
//     }

//     return NULL;
// }
// #endif

// static void* PushThread(void* arg)
// {
//     int dma_idx;

//     (void)arg;

//     while (!g_exit_flag) {
//         dma_idx = DmaPoolGetReadIndex();
//         if (dma_idx < 0)
//             break;

//         g_push_count++;
//         if (g_push_count <= 10)
//             printf("PushThread got dma idx=%d fd=%d\n",
//                    dma_idx,
//                    g_dma_bufs[dma_idx].fd);

//         FfmpegStreamPushDma(g_dma_bufs[dma_idx].fd,
//                             g_video_width,
//                             g_video_height,
//                             g_nv12_line_bytes,
//                             g_nv12_frame_size);

//         DmaPoolReleaseReadIndex(dma_idx);
//     }

//     return NULL;
// }

// static int RgaSubmitAndWait(PT_VideoBuf ptBuf)
// {
//     pthread_mutex_lock(&g_rga_mutex);

//     while (g_rga_job_pending && !g_exit_flag)
//         pthread_cond_wait(&g_rga_done_cond, &g_rga_mutex);

//     if (g_exit_flag) {
//         pthread_mutex_unlock(&g_rga_mutex);
//         return -1;
//     }

//     g_rga_job = *ptBuf;
//     g_rga_job_done = 0;
//     g_rga_job_pending = 1;
//     pthread_cond_signal(&g_rga_job_cond);

//     while (!g_rga_job_done && !g_exit_flag)
//         pthread_cond_wait(&g_rga_done_cond, &g_rga_mutex);

//     pthread_mutex_unlock(&g_rga_mutex);
//     return g_exit_flag ? -1 : 0;
// }

// int main(int argc ,char** argv)
// {
//     int iError;
//     int iPixelFormatOfVideo;
//     T_VideoDevice tVideoDevice;
//     T_VideoBuf tVideoBuf;

//     if (argc != 2) {
//         printf("Usage:\n%s </dev/video0>\n", argv[0]);
//         return -1;
//     }

//     VideoInit();

//     iError = VideoDeviceInit(argv[1], &tVideoDevice);
//     if (iError)
//         return -1;

//     iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);
//     if (iPixelFormatOfVideo != V4L2_PIX_FMT_YUYV) {
//         printf("RGA thread currently supports YUYV input only\n");
//         return -1;
//     }

//     iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
//     if (iError)
//         return -1;

//     memset(&tVideoBuf, 0, sizeof(tVideoBuf));

//     g_video_width = tVideoDevice.iWidth;
//     g_video_height = tVideoDevice.iHeight;
//     g_nv12_line_bytes = (g_video_width + 15) & ~15;
//     g_nv12_frame_size = g_nv12_line_bytes * g_video_height * 3 / 2;

//     printf("capture %dx%d, nv12 stride=%d size=%d\n",
//            g_video_width,
//            g_video_height,
//            g_nv12_line_bytes,
//            g_nv12_frame_size);

//     if (DmaPoolInit() != 0)
//         return -1;

// #if INFER_ENABLE
//     if (InferDmaPoolInit() != 0)
//         return -1;

//     if (InferBridgeInit(INFER_INPUT_WIDTH,
//                         INFER_INPUT_HEIGHT,
//                         g_infer_rgb_line_bytes) != 0)
//         return -1;
// #else
//     printf("Infer disabled by INFER_ENABLE=0, streaming only\n");
// #endif

//     iError = FfmpegStreamInit("rtmp://192.168.31.153:1935/live/test",
//                               tVideoDevice.iWidth,
//                               tVideoDevice.iHeight,
//                               V4L2_PIX_FMT_NV12,
//                               FFMPEG_STREAM_RTMP,
//                               25);
//     if (iError)
//         return -1;

//     if (pthread_create(&g_rga_thread, NULL, RgaThread, NULL) != 0)
//         return -1;

//     if (pthread_create(&g_push_thread, NULL, PushThread, NULL) != 0)
//         return -1;

// #if INFER_ENABLE
//     if (pthread_create(&g_infer_thread, NULL, InferThread, NULL) != 0)
//         return -1;
// #endif

//     while (1) {
//         iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
//         if (iError)
//             return -1;

//         RgaSubmitAndWait(&tVideoBuf);

//         iError = tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);
//         if (iError)
//             return -1;
//     }

//     return 0;
// }
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include <config.h>
#include <video_manager.h>
#include <Ffmpeg_stream.h>
#include <infer_bridge.h>
#include <dmabuf_manager.h>

#include "rga/im2d.h"
#include "rga/rga.h"

#define RGA_DMA_BUF_COUNT 2
#define INFER_DMA_BUF_COUNT 2
#define INFER_INPUT_WIDTH 224
#define INFER_INPUT_HEIGHT 224
#define INFER_INTERVAL_MS 500

static int g_video_width;
static int g_video_height;
static int g_nv12_line_bytes;
static int g_nv12_frame_size;
static int g_infer_rgb_line_bytes;
static int g_infer_rgb_frame_size;
static pthread_t g_rga_thread;
static pthread_t g_push_thread;
#if INFER_ENABLE
static pthread_t g_infer_thread;
#endif
static volatile int g_exit_flag = 0;

static T_DmaBufPool g_stream_dma_pool;

#if INFER_ENABLE
static T_DmaBufPool g_infer_dma_pool;
#endif

static T_VideoBuf g_rga_job;
static int g_rga_job_pending = 0;
static int g_rga_job_done = 0;
static pthread_mutex_t g_rga_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_rga_job_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_rga_done_cond = PTHREAD_COND_INITIALIZER;

static int g_rga_count = 0;
static int g_push_count = 0;

static long long GetMonoMs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int ShouldRunInfer(void)
{
    static long long last_infer_ms = 0;
    long long now_ms = GetMonoMs();

    if (last_infer_ms == 0 || now_ms - last_infer_ms >= INFER_INTERVAL_MS) {
        last_infer_ms = now_ms;
        return 1;
    }

    return 0;
}

static int DmaPoolInit(void)
{
    return DmaBufPoolInit(&g_stream_dma_pool,
                          RGA_DMA_BUF_COUNT,
                          g_nv12_frame_size,
                          "RGA NV12",
                          &g_exit_flag);
}

#if INFER_ENABLE
static int InferDmaPoolInit(void)
{
    g_infer_rgb_line_bytes = INFER_INPUT_WIDTH * 3;
    g_infer_rgb_frame_size = g_infer_rgb_line_bytes * INFER_INPUT_HEIGHT;

    return DmaBufPoolInit(&g_infer_dma_pool,
                          INFER_DMA_BUF_COUNT,
                          g_infer_rgb_frame_size,
                          "Infer RGB888",
                          &g_exit_flag);
}
#endif

static int RgaYuyvToNv12(void *src_buf, int dst_idx)
{
    static int rga_debug_count = 0;
    T_DmaBuf *dst_buf;
    rga_buffer_t src;
    rga_buffer_t dst;
    int ret;

    dst_buf = DmaBufPoolGetBuf(&g_stream_dma_pool, dst_idx);
    if (!dst_buf)
        return -1;

    src = wrapbuffer_virtualaddr(src_buf,
                                 g_video_width,
                                 g_video_height,
                                 RK_FORMAT_YUYV_422);
    dst = wrapbuffer_fd(dst_buf->fd,
                        g_video_width,
                        g_video_height,
                        RK_FORMAT_YCbCr_420_SP);

    if (rga_debug_count < 10) {
        printf("RGA convert begin src=%p dst_idx=%d dst_fd=%d %dx%d size=%d\n",
               src_buf,
               dst_idx,
               dst_buf->fd,
               g_video_width,
               g_video_height,
               g_nv12_frame_size);
        fflush(stdout);
    }

    ret = imcvtcolor(src, dst,
                     RK_FORMAT_YUYV_422,
                     RK_FORMAT_YCbCr_420_SP);

    if (rga_debug_count < 10) {
        printf("RGA convert ret=%d\n", ret);
        fflush(stdout);
        rga_debug_count++;
    }

    if (ret != IM_STATUS_SUCCESS) {
        printf("RGA convert failed: %d\n", ret);
        fflush(stdout);
        return -1;
    }

    return 0;
}

#if INFER_ENABLE
static int RgaYuyvToRgb888Infer(void *src_buf, int dst_idx)
{
    static int infer_debug_count = 0;
    T_DmaBuf *dst_buf;
    rga_buffer_t src;
    rga_buffer_t dst;
    int ret;

    dst_buf = DmaBufPoolGetBuf(&g_infer_dma_pool, dst_idx);
    if (!dst_buf)
        return -1;

    src = wrapbuffer_virtualaddr(src_buf,
                                 g_video_width,
                                 g_video_height,
                                 RK_FORMAT_YUYV_422);
    dst = wrapbuffer_fd(dst_buf->fd,
                        INFER_INPUT_WIDTH,
                        INFER_INPUT_HEIGHT,
                        RK_FORMAT_RGB_888);

    ret = imcvtcolor(src, dst,
                     RK_FORMAT_YUYV_422,
                     RK_FORMAT_RGB_888);

    if (infer_debug_count < 10) {
        printf("RGA infer convert ret=%d src=%dx%d dst_rgb=%dx%d fd=%d\n",
               ret,
               g_video_width,
               g_video_height,
               INFER_INPUT_WIDTH,
               INFER_INPUT_HEIGHT,
               dst_buf->fd);
        fflush(stdout);
        infer_debug_count++;
    }

    if (ret != IM_STATUS_SUCCESS) {
        printf("RGA infer RGB888 convert failed: %d\n", ret);
        fflush(stdout);
        return -1;
    }

    return 0;
}
#endif

static void* RgaThread(void* arg)
{
    T_VideoBuf raw_buf;
    int dma_idx;
#if INFER_ENABLE
    int infer_idx;
#endif

    (void)arg;
    memset(&raw_buf, 0, sizeof(raw_buf));

    while (!g_exit_flag) {
        pthread_mutex_lock(&g_rga_mutex);
        while (!g_rga_job_pending && !g_exit_flag)
            pthread_cond_wait(&g_rga_job_cond, &g_rga_mutex);

        if (g_exit_flag) {
            pthread_mutex_unlock(&g_rga_mutex);
            break;
        }

        raw_buf = g_rga_job;
        g_rga_job_pending = 0;
        pthread_mutex_unlock(&g_rga_mutex);

        dma_idx = DmaBufPoolGetWriteIndex(&g_stream_dma_pool, 1, 1);
        if (dma_idx < 0)
            break;

        if (RgaYuyvToNv12(raw_buf.tPixelDatas.aucPixelDatas, dma_idx) == 0) {
            g_rga_count++;
            DmaBufPoolPublish(&g_stream_dma_pool, dma_idx, g_rga_count, 1);
#if INFER_ENABLE
            if (ShouldRunInfer()) {
                infer_idx = DmaBufPoolGetWriteIndex(&g_infer_dma_pool, 0, 1);
                if (infer_idx >= 0) {
                    if (RgaYuyvToRgb888Infer(raw_buf.tPixelDatas.aucPixelDatas,
                                             infer_idx) == 0) {
                        DmaBufPoolPublish(&g_infer_dma_pool, infer_idx, g_rga_count, 1);
                    } else {
                        DmaBufPoolReleaseIndex(&g_infer_dma_pool, infer_idx);
                    }
                }
            }
#endif
        } else {
            DmaBufPoolReleaseIndex(&g_stream_dma_pool, dma_idx);
        }

        pthread_mutex_lock(&g_rga_mutex);
        g_rga_job_done = 1;
        pthread_cond_signal(&g_rga_done_cond);
        pthread_mutex_unlock(&g_rga_mutex);
    }

    return NULL;
}

#if INFER_ENABLE
static void* InferThread(void* arg)
{
    int infer_idx;
    int count = 0;

    (void)arg;

    while (!g_exit_flag) {
        T_DmaBuf *infer_buf;

        infer_idx = DmaBufPoolGetReadIndex(&g_infer_dma_pool);
        if (infer_idx < 0)
            break;

        infer_buf = DmaBufPoolGetBuf(&g_infer_dma_pool, infer_idx);
        if (!infer_buf) {
            DmaBufPoolReleaseIndex(&g_infer_dma_pool, infer_idx);
            continue;
        }

        count++;
        if (count <= 10)
            printf("InferThread got rgb dma idx=%d fd=%d frame=%lld\n",
                   infer_idx,
                   infer_buf->fd,
                   (long long)infer_buf->frame_id);

        InferBridgeFeedRgb888Dma(infer_buf->fd,
                                 INFER_INPUT_WIDTH,
                                 INFER_INPUT_HEIGHT,
                                 g_infer_rgb_line_bytes,
                                 g_infer_rgb_frame_size,
                                 infer_buf->frame_id);

        DmaBufPoolReleaseIndex(&g_infer_dma_pool, infer_idx);
    }

    return NULL;
}
#endif

static void* PushThread(void* arg)
{
    int dma_idx;

    (void)arg;

    while (!g_exit_flag) {
        T_DmaBuf *stream_buf;

        dma_idx = DmaBufPoolGetReadIndex(&g_stream_dma_pool);
        if (dma_idx < 0)
            break;

        stream_buf = DmaBufPoolGetBuf(&g_stream_dma_pool, dma_idx);
        if (!stream_buf) {
            DmaBufPoolReleaseIndex(&g_stream_dma_pool, dma_idx);
            continue;
        }

        g_push_count++;
        if (g_push_count <= 10)
            printf("PushThread got dma idx=%d fd=%d\n",
                   dma_idx,
                   stream_buf->fd);

        FfmpegStreamPushDma(stream_buf->fd,
                            g_video_width,
                            g_video_height,
                            g_nv12_line_bytes,
                            g_nv12_frame_size);

        DmaBufPoolReleaseIndex(&g_stream_dma_pool, dma_idx);
    }

    return NULL;
}

static int RgaSubmitAndWait(PT_VideoBuf ptBuf)
{
    pthread_mutex_lock(&g_rga_mutex);

    while (g_rga_job_pending && !g_exit_flag)
        pthread_cond_wait(&g_rga_done_cond, &g_rga_mutex);

    if (g_exit_flag) {
        pthread_mutex_unlock(&g_rga_mutex);
        return -1;
    }

    g_rga_job = *ptBuf;
    g_rga_job_done = 0;
    g_rga_job_pending = 1;
    pthread_cond_signal(&g_rga_job_cond);

    while (!g_rga_job_done && !g_exit_flag)
        pthread_cond_wait(&g_rga_done_cond, &g_rga_mutex);

    pthread_mutex_unlock(&g_rga_mutex);
    return g_exit_flag ? -1 : 0;
}

int main(int argc ,char** argv)
{
    int iError;
    int iPixelFormatOfVideo;
    T_VideoDevice tVideoDevice;
    T_VideoBuf tVideoBuf;

    if (argc != 2) {
        printf("Usage:\n%s </dev/video0>\n", argv[0]);
        return -1;
    }

    VideoInit();

    iError = VideoDeviceInit(argv[1], &tVideoDevice);
    if (iError)
        return -1;

    iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);
    if (iPixelFormatOfVideo != V4L2_PIX_FMT_YUYV) {
        printf("RGA thread currently supports YUYV input only\n");
        return -1;
    }

    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
        return -1;

    memset(&tVideoBuf, 0, sizeof(tVideoBuf));

    g_video_width = tVideoDevice.iWidth;
    g_video_height = tVideoDevice.iHeight;
    g_nv12_line_bytes = (g_video_width + 15) & ~15;
    g_nv12_frame_size = g_nv12_line_bytes * g_video_height * 3 / 2;

    printf("capture %dx%d, nv12 stride=%d size=%d\n",
           g_video_width,
           g_video_height,
           g_nv12_line_bytes,
           g_nv12_frame_size);

    if (DmaPoolInit() != 0)
        return -1;

#if INFER_ENABLE
    if (InferDmaPoolInit() != 0)
        return -1;

    if (InferBridgeInit(INFER_INPUT_WIDTH,
                        INFER_INPUT_HEIGHT,
                        g_infer_rgb_line_bytes) != 0)
        return -1;
#else
    printf("Infer disabled by INFER_ENABLE=0, streaming only\n");
#endif

    iError = FfmpegStreamInit("rtmp://192.168.31.153:1935/live/test",
                              tVideoDevice.iWidth,
                              tVideoDevice.iHeight,
                              V4L2_PIX_FMT_NV12,
                              FFMPEG_STREAM_RTMP,
                              60);
    if (iError)
        return -1;

    if (pthread_create(&g_rga_thread, NULL, RgaThread, NULL) != 0)
        return -1;

    if (pthread_create(&g_push_thread, NULL, PushThread, NULL) != 0)
        return -1;

#if INFER_ENABLE
    if (pthread_create(&g_infer_thread, NULL, InferThread, NULL) != 0)
        return -1;
#endif

    while (1) {
        iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
            return -1;

        RgaSubmitAndWait(&tVideoBuf);

        iError = tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
            return -1;
    }

    return 0;
}
