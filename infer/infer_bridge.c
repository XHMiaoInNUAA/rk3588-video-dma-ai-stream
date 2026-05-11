// #include <stdio.h>
// #include <stdint.h>
// #include <sys/mman.h>
// #include <unistd.h>

// #include "config.h"
// #include "infer_bridge.h"
// #include "infer_llm.h"

// typedef struct {
//     int inited;
//     int input_w;
//     int input_h;
//     int input_stride;
//     int feed_count;
//     int mapped_fd;
//     int mapped_size;
//     uint8_t *mapped_ptr;
// } T_InferBridgeCtx;

// static T_InferBridgeCtx g_infer_ctx;

// int InferBridgeInit(int input_w, int input_h, int input_stride)
// {
//     T_InferLlmConfig cfg;
//     int ret;

//     g_infer_ctx.inited = 1;
//     g_infer_ctx.input_w = input_w;
//     g_infer_ctx.input_h = input_h;
//     g_infer_ctx.input_stride = input_stride;
//     g_infer_ctx.feed_count = 0;
//     g_infer_ctx.mapped_fd = -1;
//     g_infer_ctx.mapped_size = 0;
//     g_infer_ctx.mapped_ptr = NULL;

//     printf("InferBridge init: RGB888 input=%dx%d stride=%d, model=[1,224,224,3] NHWC FP16 (stub)\n",
//            input_w,
//            input_h,
//            input_stride);

//     cfg.encoder_model_path = INFER_ENCODER_MODEL_PATH;
//     cfg.llm_model_path = INFER_LLM_MODEL_PATH;
//     cfg.max_new_tokens = INFER_MAX_NEW_TOKENS;
//     cfg.max_context_len = INFER_MAX_CONTEXT_LEN;
//     cfg.rknn_core_num = INFER_RKNN_CORE_NUM;
//     cfg.prompt = INFER_PROMPT;
//     ret = InferLlmInit(&cfg);
//     if (ret != 0)
//         return ret;

//     return 0;
// }

// static uint8_t *InferBridgeMapDma(int dma_fd, int size)
// {
//     if (g_infer_ctx.mapped_ptr &&
//         g_infer_ctx.mapped_fd == dma_fd &&
//         g_infer_ctx.mapped_size >= size)
//         return g_infer_ctx.mapped_ptr;

//     if (g_infer_ctx.mapped_ptr) {
//         munmap(g_infer_ctx.mapped_ptr, g_infer_ctx.mapped_size);
//         g_infer_ctx.mapped_ptr = NULL;
//         g_infer_ctx.mapped_fd = -1;
//         g_infer_ctx.mapped_size = 0;
//     }

//     g_infer_ctx.mapped_ptr = mmap(NULL,
//                                   size,
//                                   PROT_READ | PROT_WRITE,
//                                   MAP_SHARED,
//                                   dma_fd,
//                                   0);
//     if (g_infer_ctx.mapped_ptr == MAP_FAILED) {
//         perror("mmap infer rgb dma");
//         g_infer_ctx.mapped_ptr = NULL;
//         return NULL;
//     }

//     g_infer_ctx.mapped_fd = dma_fd;
//     g_infer_ctx.mapped_size = size;
//     return g_infer_ctx.mapped_ptr;
// }

// int InferBridgeFeedRgb888Dma(int dma_fd,
//                              int width,
//                              int height,
//                              int stride,
//                              int size,
//                              int64_t frame_id)
// {
//     uint8_t *rgb;

//     if (!g_infer_ctx.inited)
//         return -1;

//     rgb = InferBridgeMapDma(dma_fd, size);
//     if (!rgb)
//         return -1;

//     g_infer_ctx.feed_count++;
//     if (g_infer_ctx.feed_count <= 5) {
//         printf("Infer RGB888 feed stub: frame=%lld dma_fd=%d %dx%d stride=%d size=%d\n",
//                (long long)frame_id,
//                dma_fd,
//                width,
//                height,
//                stride,
//                size);
//     }

//     return InferLlmFeedRgb888(rgb, width, height, stride, frame_id);
// }

// void InferBridgeExit(void)
// {
//     if (!g_infer_ctx.inited)
//         return;

//     if (g_infer_ctx.mapped_ptr) {
//         munmap(g_infer_ctx.mapped_ptr, g_infer_ctx.mapped_size);
//         g_infer_ctx.mapped_ptr = NULL;
//     }

//     InferLlmExit();

//     printf("InferBridge exit, total feed=%d\n", g_infer_ctx.feed_count);
//     g_infer_ctx.inited = 0;
// }
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"
#include "infer_bridge.h"
#include "infer_llm.h"

typedef struct {
    int inited;
    int input_w;
    int input_h;
    int input_stride;
    int feed_count;
    int mapped_fd;
    int mapped_size;
    uint8_t *mapped_ptr;
    uint8_t *prev_rgb;
    int prev_valid;
    int prev_size;
    int diff_drop_count;
    int diff_pass_count;
} T_InferBridgeCtx;

static T_InferBridgeCtx g_infer_ctx;

int InferBridgeInit(int input_w, int input_h, int input_stride)
{
    T_InferLlmConfig cfg;
    int ret;

    g_infer_ctx.inited = 1;
    g_infer_ctx.input_w = input_w;
    g_infer_ctx.input_h = input_h;
    g_infer_ctx.input_stride = input_stride;
    g_infer_ctx.feed_count = 0;
    g_infer_ctx.mapped_fd = -1;
    g_infer_ctx.mapped_size = 0;
    g_infer_ctx.mapped_ptr = NULL;
    g_infer_ctx.prev_size = input_stride * input_h;
    g_infer_ctx.prev_rgb = (uint8_t *)malloc(g_infer_ctx.prev_size);
    g_infer_ctx.prev_valid = 0;
    g_infer_ctx.diff_drop_count = 0;
    g_infer_ctx.diff_pass_count = 0;

    if (!g_infer_ctx.prev_rgb) {
        printf("InferBridge malloc prev_rgb failed\n");
        return -1;
    }

    printf("InferBridge init: RGB888 input=%dx%d stride=%d, model=[1,224,224,3] NHWC FP16 (stub)\n",
           input_w,
           input_h,
           input_stride);

    cfg.encoder_model_path = INFER_ENCODER_MODEL_PATH;
    cfg.llm_model_path = INFER_LLM_MODEL_PATH;
    cfg.max_new_tokens = INFER_MAX_NEW_TOKENS;
    cfg.max_context_len = INFER_MAX_CONTEXT_LEN;
    cfg.rknn_core_num = INFER_RKNN_CORE_NUM;
    cfg.prompt = INFER_PROMPT;
    ret = InferLlmInit(&cfg);
    if (ret != 0)
        return ret;

    return 0;
}

static void CopyRgbFrame(uint8_t *dst,
                         const uint8_t *src,
                         int width,
                         int height,
                         int src_stride,
                         int dst_stride)
{
    int row_bytes = width * 3;

    for (int y = 0; y < height; y++)
        memcpy(dst + y * dst_stride, src + y * src_stride, row_bytes);
}

static int FrameDiffShouldSend(const uint8_t *rgb,
                               int width,
                               int height,
                               int stride,
                               int64_t frame_id)
{
#if INFER_FRAME_DIFF_ENABLE
    int changed_pixels = 0;
    int total_pixels = width * height;
    float ratio;

    if (!g_infer_ctx.prev_valid) {
        CopyRgbFrame(g_infer_ctx.prev_rgb,
                     rgb,
                     width,
                     height,
                     stride,
                     g_infer_ctx.input_stride);
        g_infer_ctx.prev_valid = 1;
        printf("FrameDiff baseline frame=%lld, send first frame\n",
               (long long)frame_id);
        return 1;
    }

    for (int y = 0; y < height; y++) {
        const uint8_t *cur = rgb + y * stride;
        const uint8_t *prev = g_infer_ctx.prev_rgb + y * g_infer_ctx.input_stride;

        for (int x = 0; x < width; x++) {
            int off = x * 3;
            int dr = abs((int)cur[off + 0] - (int)prev[off + 0]);
            int dg = abs((int)cur[off + 1] - (int)prev[off + 1]);
            int db = abs((int)cur[off + 2] - (int)prev[off + 2]);

            if (dr + dg + db > INFER_FRAME_DIFF_PIXEL_THRESHOLD)
                changed_pixels++;
        }
    }

    CopyRgbFrame(g_infer_ctx.prev_rgb,
                 rgb,
                 width,
                 height,
                 stride,
                 g_infer_ctx.input_stride);

    ratio = (float)changed_pixels / (float)total_pixels;
    if (ratio >= INFER_FRAME_DIFF_RATIO_THRESHOLD) {
        g_infer_ctx.diff_pass_count++;
        if (g_infer_ctx.diff_pass_count <= 10)
            printf("FrameDiff pass frame=%lld changed=%d ratio=%.4f\n",
                   (long long)frame_id,
                   changed_pixels,
                   ratio);
        return 1;
    }

    g_infer_ctx.diff_drop_count++;
    if (g_infer_ctx.diff_drop_count <= 10 ||
        g_infer_ctx.diff_drop_count % 50 == 0) {
        printf("FrameDiff drop frame=%lld changed=%d ratio=%.4f drop=%d\n",
               (long long)frame_id,
               changed_pixels,
               ratio,
               g_infer_ctx.diff_drop_count);
    }
    return 0;
#else
    (void)rgb;
    (void)width;
    (void)height;
    (void)stride;
    (void)frame_id;
    return 1;
#endif
}

static uint8_t *InferBridgeMapDma(int dma_fd, int size)
{
    if (g_infer_ctx.mapped_ptr &&
        g_infer_ctx.mapped_fd == dma_fd &&
        g_infer_ctx.mapped_size >= size)
        return g_infer_ctx.mapped_ptr;

    if (g_infer_ctx.mapped_ptr) {
        munmap(g_infer_ctx.mapped_ptr, g_infer_ctx.mapped_size);
        g_infer_ctx.mapped_ptr = NULL;
        g_infer_ctx.mapped_fd = -1;
        g_infer_ctx.mapped_size = 0;
    }

    g_infer_ctx.mapped_ptr = mmap(NULL,
                                  size,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  dma_fd,
                                  0);
    if (g_infer_ctx.mapped_ptr == MAP_FAILED) {
        perror("mmap infer rgb dma");
        g_infer_ctx.mapped_ptr = NULL;
        return NULL;
    }

    g_infer_ctx.mapped_fd = dma_fd;
    g_infer_ctx.mapped_size = size;
    return g_infer_ctx.mapped_ptr;
}

int InferBridgeFeedRgb888Dma(int dma_fd,
                             int width,
                             int height,
                             int stride,
                             int size,
                             int64_t frame_id)
{
    uint8_t *rgb;

    if (!g_infer_ctx.inited)
        return -1;

    rgb = InferBridgeMapDma(dma_fd, size);
    if (!rgb)
        return -1;

    g_infer_ctx.feed_count++;
    if (g_infer_ctx.feed_count <= 5) {
        printf("Infer RGB888 feed stub: frame=%lld dma_fd=%d %dx%d stride=%d size=%d\n",
               (long long)frame_id,
               dma_fd,
               width,
               height,
               stride,
               size);
    }

    if (!FrameDiffShouldSend(rgb, width, height, stride, frame_id))
        return 0;

    return InferLlmFeedRgb888(rgb, width, height, stride, frame_id);
}

void InferBridgeExit(void)
{
    if (!g_infer_ctx.inited)
        return;

    if (g_infer_ctx.mapped_ptr) {
        munmap(g_infer_ctx.mapped_ptr, g_infer_ctx.mapped_size);
        g_infer_ctx.mapped_ptr = NULL;
    }

    if (g_infer_ctx.prev_rgb) {
        free(g_infer_ctx.prev_rgb);
        g_infer_ctx.prev_rgb = NULL;
    }

    InferLlmExit();

    printf("InferBridge exit, total feed=%d\n", g_infer_ctx.feed_count);
    g_infer_ctx.inited = 0;
}
