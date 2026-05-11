#ifndef _INFER_BRIDGE_H
#define _INFER_BRIDGE_H

#include <stdint.h>

int InferBridgeInit(int input_w, int input_h, int input_stride);
int InferBridgeFeedRgb888Dma(int dma_fd,
                             int width,
                             int height,
                             int stride,
                             int size,
                             int64_t frame_id);
void InferBridgeExit(void);

#endif /* _INFER_BRIDGE_H */
