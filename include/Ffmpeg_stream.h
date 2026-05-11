#ifndef _FFMPEG_STREAM_H
#define _FFMPEG_STREAM_H

#include <config.h>
#include "video_manager.h"

/* 支持的推流协议类型 */
#define FFMPEG_STREAM_RTMP  0
#define FFMPEG_STREAM_RTSP  1
#define FFMPEG_STREAM_SRT   2
#define FFMPEG_STREAM_UDP   3
#define FFMPEG_STREAM_FLV   4

/* 推流模块结构体（和你的 T_VideoConvert 完全同风格） */
typedef struct FfmpegStream {
    char *name;               /* 模块名 */
    int  iProtocol;           /* 协议类型 RTMP/RTSP/SRT... */
    
    /* 判断是否支持输入像素格式 + 输出协议 */
    int  (*isSupport)(int iPixelFormatIn, int iStreamType);
    
    /* 初始化：URL + 宽 + 高 + 输入格式 + 帧率 */
    int  (*Init)(char *url, int width, int height, int iPixelFormatIn, int fps);
    
    /* 推流一帧 */
    int  (*Push)(PT_VideoBuf ptVideoBuf);
    int  (*PushDma)(int dma_fd, int width, int height, int stride, int size);
    
    /* 退出释放 */
    void (*Exit)(void);
    
    struct FfmpegStream *ptNext;
} T_FfmpegStream, *PT_FfmpegStream;

/* 对外标准接口（和你的模块完全统一） */
int FfmpegStreamInit(char *url, int width, int height, int iPixelFormatIn, int iStreamType, int fps);
int FfmpegStreamPush(PT_VideoBuf ptVideoBuf);
int FfmpegStreamPushDma(int dma_fd, int width, int height, int stride, int size);
void FfmpegStreamExit(void);
int RegisterFfmpegStream(PT_FfmpegStream ptStream);

/* 内部支持的推流器初始化 */
int RtmpStreamInit(void);
int RtspStreamInit(void);
int SrtStreamInit(void);
int RtmpStreamRegister(void);
int RtspStreamRegister(void);
int SrtStreamRegister(void);

#endif
