#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include "Ffmpeg_stream.h"

static AVFormatContext *g_ofmt_ctx = NULL;
static AVCodecContext  *g_cc = NULL;
static AVFrame         *g_frame = NULL;
static struct SwsContext *g_sws = NULL;
static AVStream        *g_st = NULL;

static int g_w, g_h;
static int64_t g_pts = 0;

static int rtsp_isSupport(int iFmtIn, int type)
{
    return (type == FFMPEG_STREAM_RTSP);
}

static int rtsp_Init(char *url, int w, int h, int iFmtIn, int fps)
{
    g_w = w;
    g_h = h;

    avformat_network_init();

    // 创建输出上下文
    avformat_alloc_output_context2(&g_ofmt_ctx, NULL, "rtsp", url);
    if (!g_ofmt_ctx) {
        printf("alloc output context failed\n");
        return -1;
    }

    // ✅ 使用 rkmpp 编码器
    const AVCodec *codec = avcodec_find_encoder_by_name("h264_rkmpp");
    if (!codec) {
        printf("cannot find h264_rkmpp\n");
        return -1;
    }

    g_st = avformat_new_stream(g_ofmt_ctx, NULL);
    if (!g_st) {
        printf("new stream failed\n");
        return -1;
    }

    g_cc = avcodec_alloc_context3(codec);

    g_cc->width = w;
    g_cc->height = h;
    g_cc->pix_fmt = AV_PIX_FMT_NV12;   // ⚠️ rkmpp推荐NV12
    g_cc->time_base = (AVRational){1, fps};
    g_cc->framerate = (AVRational){fps, 1};
    g_cc->bit_rate = 1000000;

    // 关键：低延迟
    g_cc->gop_size = fps;
    g_cc->max_b_frames = 0;

    if (g_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        g_cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(g_cc, codec, NULL) < 0) {
        printf("open codec failed\n");
        return -1;
    }

    avcodec_parameters_from_context(g_st->codecpar, g_cc);

    // 打开RTSP
    if (!(g_ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&g_ofmt_ctx->pb, url, AVIO_FLAG_WRITE) < 0) {
            printf("avio open failed\n");
            return -1;
        }
    }

    if (avformat_write_header(g_ofmt_ctx, NULL) < 0) {
        printf("write header failed\n");
        return -1;
    }

    // 分配frame（NV12）
    g_frame = av_frame_alloc();
    g_frame->format = AV_PIX_FMT_NV12;
    g_frame->width = w;
    g_frame->height = h;

    av_image_alloc(g_frame->data, g_frame->linesize, w, h, AV_PIX_FMT_NV12, 1);

    // YUYV422 → NV12
    g_sws = sws_getContext(
        w, h, AV_PIX_FMT_YUYV422,
        w, h, AV_PIX_FMT_NV12,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    return 0;
}

static int rtsp_Push(PT_VideoBuf ptBuf)
{
    uint8_t *src[] = { ptBuf->tPixelDatas.aucPixelDatas };
    int stride[] = { ptBuf->tPixelDatas.iWidth * 2 };

    // 转格式
    sws_scale(g_sws, src, stride, 0, g_h,
              g_frame->data, g_frame->linesize);

    g_frame->pts = g_pts++;

    // ✅ send frame
    int ret = avcodec_send_frame(g_cc, g_frame);
    if (ret < 0) {
        printf("send frame error\n");
        return -1;
    }

    AVPacket pkt;
    av_init_packet(&pkt);

    // ✅ receive packet
    while (ret >= 0) {
        ret = avcodec_receive_packet(g_cc, &pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
            return -1;

        pkt.stream_index = g_st->index;

        av_packet_rescale_ts(&pkt, g_cc->time_base, g_st->time_base);

        av_interleaved_write_frame(g_ofmt_ctx, &pkt);

        av_packet_unref(&pkt);
    }

    return 0;
}

static void rtsp_Exit(void)
{
    av_write_trailer(g_ofmt_ctx);

    if (g_frame) {
        av_freep(&g_frame->data[0]);
        av_frame_free(&g_frame);
    }

    if (g_cc)
        avcodec_free_context(&g_cc);

    if (g_sws)
        sws_freeContext(g_sws);

    if (!(g_ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&g_ofmt_ctx->pb);

    if (g_ofmt_ctx)
        avformat_free_context(g_ofmt_ctx);
}

static T_FfmpegStream g_rtspStream = {
    .name = "rtsp_stream",
    .iProtocol = FFMPEG_STREAM_RTSP,
    .isSupport = rtsp_isSupport,
    .Init = rtsp_Init,
    .Push = rtsp_Push,
    .Exit = rtsp_Exit,
};

int RtspStreamRegister(void)
{
    return RegisterFfmpegStream(&g_rtspStream);
}