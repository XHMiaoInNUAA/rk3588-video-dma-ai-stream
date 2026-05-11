#ifndef _INFER_LLM_H
#define _INFER_LLM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *encoder_model_path;
    const char *llm_model_path;
    int max_new_tokens;
    int max_context_len;
    int rknn_core_num;
    const char *prompt;
} T_InferLlmConfig;

int InferLlmInit(const T_InferLlmConfig *cfg);
int InferLlmFeedRgb888(const uint8_t *rgb,
                       int width,
                       int height,
                       int stride,
                       int64_t frame_id);
void InferLlmExit(void);

#ifdef __cplusplus
}
#endif

#endif /* _INFER_LLM_H */
