#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Ffmpeg_stream.h"

static PT_FfmpegStream g_ptFfmpegStream = NULL;
static PT_FfmpegStream g_ptSelected = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

int RegisterFfmpegStream(PT_FfmpegStream ptStream)
{
    PT_FfmpegStream ptTmp;

    if (!g_ptFfmpegStream) {
        g_ptFfmpegStream = ptStream;
        ptStream->ptNext = NULL;
    } else {
        ptTmp = g_ptFfmpegStream;
        while (ptTmp->ptNext)
            ptTmp = ptTmp->ptNext;
        ptTmp->ptNext = ptStream;
        ptStream->ptNext = NULL;
    }
    return 0;
}

static PT_FfmpegStream GetStream(int iFmtIn, int iStreamType)
{
    PT_FfmpegStream ptTmp = g_ptFfmpegStream;
    while (ptTmp) {
        if (ptTmp->isSupport(iFmtIn, iStreamType))
            return ptTmp;
        ptTmp = ptTmp->ptNext;
    }
    return NULL;
}

int FfmpegStreamInit(char *url, int w, int h, int iFmtIn, int iStreamType, int fps)
{
    printf("输入格式 iFmtIn = %d\n", iFmtIn);
    pthread_mutex_lock(&g_mutex);

    RtmpStreamRegister();
    RtspStreamRegister();
    SrtStreamRegister();

    g_ptSelected = GetStream(iFmtIn, iStreamType);
    if (!g_ptSelected) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    int ret = g_ptSelected->Init(url, w, h, iFmtIn, fps);
    pthread_mutex_unlock(&g_mutex);
    return ret;
}

int FfmpegStreamPush(PT_VideoBuf ptBuf)
{
    int ret = -1;
    pthread_mutex_lock(&g_mutex);
    if (g_ptSelected && g_ptSelected->Push)
        ret = g_ptSelected->Push(ptBuf);
    pthread_mutex_unlock(&g_mutex);
    return ret;
}

int FfmpegStreamPushDma(int dma_fd, int width, int height, int stride, int size)
{
    int ret = -1;
    pthread_mutex_lock(&g_mutex);
    if (g_ptSelected && g_ptSelected->PushDma)
        ret = g_ptSelected->PushDma(dma_fd, width, height, stride, size);
    pthread_mutex_unlock(&g_mutex);
    return ret;
}

void FfmpegStreamExit(void)
{
    pthread_mutex_lock(&g_mutex);
    if (g_ptSelected && g_ptSelected->Exit)
        g_ptSelected->Exit();
    g_ptSelected = NULL;
    pthread_mutex_unlock(&g_mutex);
    pthread_mutex_destroy(&g_mutex);
}