#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "rga/im2d.h"
#include "rga/rga.h"

#include "Ffmpeg_stream.h"

static AVFormatContext *g_ofmt_ctx = NULL;
static AVCodecContext  *g_cc = NULL;
static AVFrame         *g_frame = NULL;
static AVStream        *g_st = NULL;

static int g_w, g_h;
static int64_t g_pts = 0;

// NV12缓存
static uint8_t *g_nv12_buf = NULL;

// 输入格式
static int g_rga_src_fmt = -1;

/*-------------------------------------------
 * 输入格式定义（按你工程来）
 *------------------------------------------*/
#define PIXEL_FMT_YUYV422   0
#define PIXEL_FMT_NV12      1
#define PIXEL_FMT_RGB888    2
#define PIXEL_FMT_YUV420P   3

/*-------------------------------------------
 * 格式映射
 *------------------------------------------*/
static int get_rga_format(int iFmtIn)
{
    switch (iFmtIn)
    {
        case PIXEL_FMT_YUYV422:
            return RK_FORMAT_YUYV_422;

        case PIXEL_FMT_NV12:
            return RK_FORMAT_YCbCr_420_SP;

        case PIXEL_FMT_RGB888:
            return RK_FORMAT_RGB_888;

        case PIXEL_FMT_YUV420P:
            return RK_FORMAT_YCbCr_420_P;

        default:
            printf("不支持格式: %d\n", iFmtIn);
            return -1;
    }
}

/*-------------------------------------------
 * RGA 转 NV12
 *------------------------------------------*/
static int rga_convert(void *src_buf, void *dst_buf)
{
    rga_buffer_t src, dst;

    src = wrapbuffer_virtualaddr(src_buf, g_w, g_h, g_rga_src_fmt);
    dst = wrapbuffer_virtualaddr(dst_buf, g_w, g_h, RK_FORMAT_YCbCr_420_SP);

    int ret = imcvtcolor(src, dst,
                         g_rga_src_fmt,
                         RK_FORMAT_YCbCr_420_SP);

    if (ret != IM_STATUS_SUCCESS) {
        printf("RGA转换失败\n");
        return -1;
    }
    return 0;
}

/*-------------------------------------------*/
static int srt_isSupport(int iFmtIn, int type)
{
    return (type == FFMPEG_STREAM_SRT);
}

/*-------------------------------------------*/
static int srt_Init(char *url, int w, int h, int iFmtIn, int fps)
{
    int ret;

    g_w = w;
    g_h = h;
    g_pts = 0;

    avformat_network_init();

    // mpegts
    ret = avformat_alloc_output_context2(&g_ofmt_ctx, NULL, "mpegts", url);
    if (ret < 0 || !g_ofmt_ctx)
        return -1;

    const AVCodec *codec = avcodec_find_encoder_by_name("h264_rkmpp");
    if (!codec) {
        printf("找不到 rkmpp\n");
        return -1;
    }

    g_st = avformat_new_stream(g_ofmt_ctx, NULL);
    if (!g_st)
        return -1;

    g_cc = avcodec_alloc_context3(codec);

    g_cc->width = w;
    g_cc->height = h;
    g_cc->pix_fmt = AV_PIX_FMT_NV12;
    g_cc->time_base = (AVRational){1, fps};
    g_cc->framerate = (AVRational){fps, 1};
    g_cc->bit_rate = 2 * 1000 * 1000;
    g_cc->gop_size = fps;
    g_cc->max_b_frames = 0;

    if (g_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        g_cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(g_cc->priv_data, "rc_mode", "CBR", 0);

    if (avcodec_open2(g_cc, codec, NULL) < 0)
        return -1;

    avcodec_parameters_from_context(g_st->codecpar, g_cc);
    g_st->time_base = g_cc->time_base;

    if (!(g_ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&g_ofmt_ctx->pb, url, AVIO_FLAG_WRITE) < 0)
            return -1;
    }

    if (avformat_write_header(g_ofmt_ctx, NULL) < 0)
        return -1;

    // 对齐
    int align_w = (w + 15) & ~15;

    g_nv12_buf = malloc(align_w * h * 3 / 2);

    g_frame = av_frame_alloc();
    g_frame->format = AV_PIX_FMT_NV12;
    g_frame->width  = w;
    g_frame->height = h;

    g_frame->data[0] = g_nv12_buf;
    g_frame->data[1] = g_nv12_buf + align_w * h;
    g_frame->linesize[0] = align_w;
    g_frame->linesize[1] = align_w;

    // 输入格式
    g_rga_src_fmt = get_rga_format(iFmtIn);
    if (g_rga_src_fmt < 0)
        return -1;

    printf("SRT推流初始化成功\n");
    return 0;
}

/*-------------------------------------------*/
static int srt_Push(PT_VideoBuf ptBuf)
{
    if (!ptBuf || !g_cc)
        return -1;

    // NV12直通
    if (g_rga_src_fmt == RK_FORMAT_YCbCr_420_SP)
    {
        memcpy(g_nv12_buf,
               ptBuf->tPixelDatas.aucPixelDatas,
               g_w * g_h * 3 / 2);
    }
    else
    {
        if (rga_convert(ptBuf->tPixelDatas.aucPixelDatas,
                        g_nv12_buf) < 0)
            return -1;
    }

    g_frame->pts = g_pts++;

    if (avcodec_send_frame(g_cc, g_frame) < 0)
        return -1;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    while (avcodec_receive_packet(g_cc, &pkt) == 0)
    {
        pkt.stream_index = g_st->index;

        av_packet_rescale_ts(&pkt,
                             g_cc->time_base,
                             g_st->time_base);

        av_interleaved_write_frame(g_ofmt_ctx, &pkt);
        av_packet_unref(&pkt);
    }

    return 0;
}

/*-------------------------------------------*/
static void srt_Exit(void)
{
    if (g_ofmt_ctx)
        av_write_trailer(g_ofmt_ctx);

    if (g_nv12_buf)
        free(g_nv12_buf);

    if (g_frame)
        av_frame_free(&g_frame);

    if (g_cc)
        avcodec_free_context(&g_cc);

    if (g_ofmt_ctx && g_ofmt_ctx->pb)
        avio_closep(&g_ofmt_ctx->pb);

    if (g_ofmt_ctx)
        avformat_free_context(g_ofmt_ctx);
}

/*-------------------------------------------*/
static T_FfmpegStream g_srtStream = {
    .name = "srt_stream",
    .iProtocol = FFMPEG_STREAM_SRT,
    .isSupport = srt_isSupport,
    .Init = srt_Init,
    .Push = srt_Push,
    .Exit = srt_Exit,
};

int SrtStreamRegister(void)
{
    return RegisterFfmpegStream(&g_srtStream);
}