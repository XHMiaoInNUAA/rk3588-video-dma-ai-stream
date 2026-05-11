#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>


//#define DBG_PRINTF(...)  
#define DBG_PRINTF printf

#define FB_DEVICE_NAME "/dev/fb0"

#define VIDEO_CAPTURE_WIDTH  640
#define VIDEO_CAPTURE_HEIGHT 480
#define INFER_ENABLE 1
#define INFER_ENCODER_MODEL_PATH "/home/orangepi/rknn-llm/examples/multimodal_model_demo/deploy/install/demo_Linux_aarch64/qwen3-vl_vision_rk3588.rknn"
#define INFER_LLM_MODEL_PATH     "/home/orangepi/rknn-llm/examples/multimodal_model_demo/deploy/install/demo_Linux_aarch64/qwen3-vl-2b-instruct_w8a8_rk3588.rkllm"
#define INFER_MAX_NEW_TOKENS     256
#define INFER_MAX_CONTEXT_LEN    2048
#define INFER_RKNN_CORE_NUM      1
#define INFER_FRAME_DIFF_ENABLE  1
#define INFER_FRAME_DIFF_PIXEL_THRESHOLD 80
#define INFER_FRAME_DIFF_RATIO_THRESHOLD 0.1f
#define INFER_PROMPT             "<image><image><image><image><image><image><image><image>比较这8帧，判断目标是否在靠近。"

#endif /* _CONFIG_H */
