// #include <signal.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #include <chrono>
// #include <string>
// #include <vector>

// #include "image_enc.h"
// #include "rkllm.h"
// #include "infer_llm.h"

// static const int FRAME_NUM = 8;

// static LLMHandle g_llm_handle = nullptr;
// static rknn_app_context_t g_rknn_app_ctx;
// static std::vector<float> g_packed_img_vec;
// static std::string g_prompt;
// static size_t g_image_width;
// static size_t g_image_height;
// static size_t g_n_image_tokens;
// static int g_single_image_embed_len;
// static int g_frame_index;
// static int g_inited;

// static int llm_callback(RKLLMResult *result, void *userdata, LLMCallState state)
// {
//     (void)userdata;

//     if (state == RKLLM_RUN_FINISH) {
//         printf("\n");
//     } else if (state == RKLLM_RUN_ERROR) {
//         printf("\nLLM run error\n");
//     } else if (state == RKLLM_RUN_NORMAL && result) {
//         printf("%s", result->text);
//     }

//     return 0;
// }

// extern "C" int InferLlmInit(const T_InferLlmConfig *cfg)
// {
//     RKLLMParam param;
//     int ret;

//     if (!cfg || !cfg->encoder_model_path || !cfg->llm_model_path ||
//         !cfg->encoder_model_path[0] || !cfg->llm_model_path[0]) {
//         printf("InferLlm disabled: set INFER_ENCODER_MODEL_PATH and INFER_LLM_MODEL_PATH\n");
//         return 0;
//     }

//     memset(&g_rknn_app_ctx, 0, sizeof(g_rknn_app_ctx));

//     param = rkllm_createDefaultParam();
//     param.model_path = cfg->llm_model_path;
//     param.top_k = 1;
//     param.max_new_tokens = cfg->max_new_tokens;
//     param.max_context_len = cfg->max_context_len;
//     param.skip_special_token = true;
//     param.extend_param.base_domain_id = 1;
//     param.img_start = "<|vision_start|>";
//     param.img_end = "<|vision_end|>";
//     param.img_content = "<|image_pad|>";

//     ret = rkllm_init(&g_llm_handle, &param, llm_callback);
//     if (ret != 0) {
//         printf("rkllm init failed, ret=%d\n", ret);
//         return -1;
//     }
//     printf("rkllm init success\n");

//     ret = init_imgenc(cfg->encoder_model_path, &g_rknn_app_ctx, cfg->rknn_core_num);
//     if (ret != 0) {
//         printf("init_imgenc fail, ret=%d model=%s\n", ret, cfg->encoder_model_path);
//         rkllm_destroy(g_llm_handle);
//         g_llm_handle = nullptr;
//         return -1;
//     }
//     printf("imgenc init success\n");

//     g_image_width = g_rknn_app_ctx.model_width;
//     g_image_height = g_rknn_app_ctx.model_height;
//     g_n_image_tokens = g_rknn_app_ctx.model_image_token;

//     g_single_image_embed_len =
//         g_rknn_app_ctx.model_image_token *
//         g_rknn_app_ctx.model_embed_size *
//         g_rknn_app_ctx.io_num.n_output;

//     g_packed_img_vec.assign(FRAME_NUM * g_single_image_embed_len, 0.0f);
//     g_prompt = cfg->prompt ? cfg->prompt : "<image><image><image><image><image><image><image><image>比较这8帧，判断目标是否在靠近。";
//     g_frame_index = 0;
//     g_inited = 1;

//     printf("InferLlm ready: imgenc_input=%zux%zu frames=%d embed_len=%d\n",
//            g_image_width,
//            g_image_height,
//            FRAME_NUM,
//            g_single_image_embed_len);

//     return 0;
// }

// static int RunLlmWithPackedImages(void)
// {
//     RKLLMInput rkllm_input;
//     RKLLMInferParam rkllm_infer_params;

//     memset(&rkllm_input, 0, sizeof(rkllm_input));
//     memset(&rkllm_infer_params, 0, sizeof(rkllm_infer_params));

//     rkllm_infer_params.mode = RKLLM_INFER_GENERATE;
//     rkllm_infer_params.keep_history = 0;

//     rkllm_input.input_type = RKLLM_INPUT_MULTIMODAL;
//     rkllm_input.role = "user";
//     rkllm_input.multimodal_input.prompt = (char *)g_prompt.c_str();
//     rkllm_input.multimodal_input.image_embed = g_packed_img_vec.data();
//     rkllm_input.multimodal_input.n_image_tokens = g_n_image_tokens;
//     rkllm_input.multimodal_input.n_image = FRAME_NUM;
//     rkllm_input.multimodal_input.image_height = g_image_height;
//     rkllm_input.multimodal_input.image_width = g_image_width;

//     printf("robot: ");
//     return rkllm_run(g_llm_handle, &rkllm_input, &rkllm_infer_params, NULL);
// }

// extern "C" int InferLlmFeedRgb888(const uint8_t *rgb,
//                                   int width,
//                                   int height,
//                                   int stride,
//                                   int64_t frame_id)
// {
//     float *dst_ptr;
//     int ret;
//     auto t0 = std::chrono::high_resolution_clock::now();

//     if (!g_inited)
//         return 0;

//     if (!rgb || width != (int)g_image_width || height != (int)g_image_height) {
//         printf("InferLlmFeedRgb888 invalid input: got=%dx%d need=%zux%zu\n",
//                width,
//                height,
//                g_image_width,
//                g_image_height);
//         return -1;
//     }

//     (void)stride;
//     dst_ptr = g_packed_img_vec.data() + g_frame_index * g_single_image_embed_len;

//     ret = run_imgenc(&g_rknn_app_ctx, (void *)rgb, dst_ptr);
//     if (ret != 0) {
//         printf("run_imgenc fail ret=%d frame=%lld slot=%d\n",
//                ret,
//                (long long)frame_id,
//                g_frame_index);
//         return -1;
//     }

//     auto t1 = std::chrono::high_resolution_clock::now();
//     auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
//     printf("imgenc frame=%lld slot=%d took %.2f ms\n",
//            (long long)frame_id,
//            g_frame_index,
//            us.count() / 1000.0);

//     g_frame_index++;
//     if (g_frame_index >= FRAME_NUM) {
//         g_frame_index = 0;
//         return RunLlmWithPackedImages();
//     }

//     return 0;
// }

// extern "C" void InferLlmExit(void)
// {
//     if (!g_inited)
//         return;

//     release_imgenc(&g_rknn_app_ctx);

//     if (g_llm_handle) {
//         rkllm_destroy(g_llm_handle);
//         g_llm_handle = nullptr;
//     }

//     g_packed_img_vec.clear();
//     g_inited = 0;
// }


#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <string>
#include <vector>

#include "image_enc.h"
#include "rkllm.h"
#include "infer_llm.h"



static const int FRAME_NUM = 8;

static LLMHandle g_llm_handle = nullptr;
static rknn_app_context_t g_rknn_app_ctx;
static std::vector<float> g_packed_img_vec;
static std::vector<float> g_llm_job_vec;
static std::string g_prompt;
static size_t g_image_width;
static size_t g_image_height;
static size_t g_n_image_tokens;
static int g_single_image_embed_len;
static int g_frame_index;
static int g_inited;

static pthread_t g_llm_thread;
static pthread_mutex_t g_llm_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_llm_cond = PTHREAD_COND_INITIALIZER;
static int g_exit_flag;
static int g_llm_job_pending;
static int g_llm_busy;

static int llm_callback(RKLLMResult *result, void *userdata, LLMCallState state)
{
    (void)userdata;

    if (state == RKLLM_RUN_FINISH) {
        printf("\n");
    } else if (state == RKLLM_RUN_ERROR) {
        printf("\nLLM run error\n");
    } else if (state == RKLLM_RUN_NORMAL && result) {
        printf("%s", result->text);
    }

    return 0;
}

static int RunLlmWithImages(const std::vector<float> &image_embed)
{
    RKLLMInput rkllm_input;
    RKLLMInferParam rkllm_infer_params;

    memset(&rkllm_input, 0, sizeof(rkllm_input));
    memset(&rkllm_infer_params, 0, sizeof(rkllm_infer_params));

    rkllm_infer_params.mode = RKLLM_INFER_GENERATE;
    rkllm_infer_params.keep_history = 0;

    rkllm_input.input_type = RKLLM_INPUT_MULTIMODAL;
    rkllm_input.role = "user";
    rkllm_input.multimodal_input.prompt = (char *)g_prompt.c_str();
    rkllm_input.multimodal_input.image_embed = (float *)image_embed.data();
    rkllm_input.multimodal_input.n_image_tokens = g_n_image_tokens;
    rkllm_input.multimodal_input.n_image = FRAME_NUM;
    rkllm_input.multimodal_input.image_height = g_image_height;
    rkllm_input.multimodal_input.image_width = g_image_width;

    printf("robot: ");
    return rkllm_run(g_llm_handle, &rkllm_input, &rkllm_infer_params, NULL);
}

static void *LlmThread(void *arg)
{
    std::vector<float> local_job;

    (void)arg;

    while (1) {
        pthread_mutex_lock(&g_llm_mutex);
        while (!g_llm_job_pending && !g_exit_flag)
            pthread_cond_wait(&g_llm_cond, &g_llm_mutex);

        if (g_exit_flag) {
            pthread_mutex_unlock(&g_llm_mutex);
            break;
        }

        local_job = g_llm_job_vec;
        g_llm_job_pending = 0;
        g_llm_busy = 1;
        pthread_mutex_unlock(&g_llm_mutex);

        RunLlmWithImages(local_job);

        pthread_mutex_lock(&g_llm_mutex);
        g_llm_busy = 0;
        pthread_mutex_unlock(&g_llm_mutex);
    }

    return NULL;
}

static void SubmitLlmJobOrDrop(void)
{
    pthread_mutex_lock(&g_llm_mutex);
    if (!g_llm_busy && !g_llm_job_pending) {
        g_llm_job_vec = g_packed_img_vec;
        g_llm_job_pending = 1;
        pthread_cond_signal(&g_llm_cond);
        printf("LLM job submitted, streaming continues\n");
    } else {
        printf("LLM busy, drop current 8-frame job\n");
    }
    pthread_mutex_unlock(&g_llm_mutex);
}

static int LlmCanAcceptImgencFrame(void)
{
    int can_accept;

    pthread_mutex_lock(&g_llm_mutex);
    can_accept = (!g_llm_busy && !g_llm_job_pending && !g_exit_flag);
    pthread_mutex_unlock(&g_llm_mutex);

    return can_accept;
}

extern "C" int InferLlmInit(const T_InferLlmConfig *cfg)
{
    RKLLMParam param;
    int ret;

    if (!cfg || !cfg->encoder_model_path || !cfg->llm_model_path ||
        !cfg->encoder_model_path[0] || !cfg->llm_model_path[0]) {
        printf("InferLlm disabled: set model paths\n");
        return 0;
    }

    memset(&g_rknn_app_ctx, 0, sizeof(g_rknn_app_ctx));

    param = rkllm_createDefaultParam();
    param.model_path = cfg->llm_model_path;
    param.top_k = 1;
    param.max_new_tokens = cfg->max_new_tokens;
    param.max_context_len = cfg->max_context_len;
    param.skip_special_token = true;
    param.extend_param.base_domain_id = 1;
    param.img_start = "<|vision_start|>";
    param.img_end = "<|vision_end|>";
    param.img_content = "<|image_pad|>";

    ret = rkllm_init(&g_llm_handle, &param, llm_callback);
    if (ret != 0) {
        printf("rkllm init failed, ret=%d\n", ret);
        return -1;
    }
    printf("rkllm init success\n");

    ret = init_imgenc(cfg->encoder_model_path, &g_rknn_app_ctx, cfg->rknn_core_num);
    if (ret != 0) {
        printf("init_imgenc fail, ret=%d model=%s\n", ret, cfg->encoder_model_path);
        rkllm_destroy(g_llm_handle);
        g_llm_handle = nullptr;
        return -1;
    }
    printf("imgenc init success\n");

    g_image_width = g_rknn_app_ctx.model_width;
    g_image_height = g_rknn_app_ctx.model_height;
    g_n_image_tokens = g_rknn_app_ctx.model_image_token;
    g_single_image_embed_len =
        g_rknn_app_ctx.model_image_token *
        g_rknn_app_ctx.model_embed_size *
        g_rknn_app_ctx.io_num.n_output;

    g_packed_img_vec.assign(FRAME_NUM * g_single_image_embed_len, 0.0f);
    g_llm_job_vec.assign(FRAME_NUM * g_single_image_embed_len, 0.0f);
    g_prompt = cfg->prompt ? cfg->prompt :
        "<image><image><image><image><image><image><image><image>compare these 8 frames.";
    g_frame_index = 0;
    g_exit_flag = 0;
    g_llm_job_pending = 0;
    g_llm_busy = 0;
    g_inited = 1;

    ret = pthread_create(&g_llm_thread, NULL, LlmThread, NULL);
    if (ret != 0) {
        printf("create LLM thread failed, ret=%d\n", ret);
        release_imgenc(&g_rknn_app_ctx);
        rkllm_destroy(g_llm_handle);
        g_llm_handle = nullptr;
        g_inited = 0;
        return -1;
    }

    printf("InferLlm ready: imgenc_input=%zux%zu frames=%d embed_len=%d\n",
           g_image_width, g_image_height, FRAME_NUM, g_single_image_embed_len);
    return 0;
}

extern "C" int InferLlmFeedRgb888(const uint8_t *rgb,
                                  int width,
                                  int height,
                                  int stride,
                                  int64_t frame_id)
{
    float *dst_ptr;
    int ret;
    auto t0 = std::chrono::high_resolution_clock::now();

    if (!g_inited)
        return 0;

    if (!rgb || width != (int)g_image_width || height != (int)g_image_height) {
        printf("InferLlmFeedRgb888 invalid input: got=%dx%d need=%zux%zu\n",
               width, height, g_image_width, g_image_height);
        return -1;
    }

    if (!LlmCanAcceptImgencFrame()) {
        static int drop_count = 0;

        drop_count++;
        if (drop_count <= 5 || drop_count % 20 == 0)
            printf("LLM busy, skip imgenc frame=%lld drop=%d\n",
                   (long long)frame_id, drop_count);
        return 0;
    }

    (void)stride;
    dst_ptr = g_packed_img_vec.data() + g_frame_index * g_single_image_embed_len;

    ret = run_imgenc(&g_rknn_app_ctx, (void *)rgb, dst_ptr);
    if (ret != 0) {
        printf("run_imgenc fail ret=%d frame=%lld slot=%d\n",
               ret, (long long)frame_id, g_frame_index);
        return -1;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    printf("imgenc frame=%lld slot=%d took %.2f ms\n",
           (long long)frame_id, g_frame_index, us.count() / 1000.0);

    g_frame_index++;
    if (g_frame_index >= FRAME_NUM) {
        g_frame_index = 0;
        SubmitLlmJobOrDrop();
    }

    return 0;
}

extern "C" void InferLlmExit(void)
{
    if (!g_inited)
        return;

    pthread_mutex_lock(&g_llm_mutex);
    g_exit_flag = 1;
    pthread_cond_signal(&g_llm_cond);
    pthread_mutex_unlock(&g_llm_mutex);
    pthread_join(g_llm_thread, NULL);

    release_imgenc(&g_rknn_app_ctx);

    if (g_llm_handle) {
        rkllm_destroy(g_llm_handle);
        g_llm_handle = nullptr;
    }

    g_packed_img_vec.clear();
    g_llm_job_vec.clear();
    g_inited = 0;
}