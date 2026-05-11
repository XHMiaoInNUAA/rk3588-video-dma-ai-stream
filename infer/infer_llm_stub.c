#include <stdio.h>

#include "infer_llm.h"

__attribute__((weak))
int InferLlmInit(const T_InferLlmConfig *cfg)
{
    if (!cfg || !cfg->encoder_model_path || !cfg->llm_model_path ||
        !cfg->encoder_model_path[0] || !cfg->llm_model_path[0]) {
        printf("InferLlm disabled: set INFER_ENCODER_MODEL_PATH and INFER_LLM_MODEL_PATH\n");
        return 0;
    }

    printf("InferLlm real backend is not linked, using stub\n");
    return 0;
}

__attribute__((weak))
int InferLlmFeedRgb888(const uint8_t *rgb,
                       int width,
                       int height,
                       int stride,
                       int64_t frame_id)
{
    (void)rgb;
    (void)width;
    (void)height;
    (void)stride;

    if (frame_id <= 5)
        printf("InferLlm stub feed frame=%lld\n", (long long)frame_id);

    return 0;
}

__attribute__((weak))
void InferLlmExit(void)
{
}
