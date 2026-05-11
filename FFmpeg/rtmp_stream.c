// #define _GNU_SOURCE
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <signal.h>
// #include <unistd.h>

// #include <libavformat/avformat.h>
// #include <libavcodec/avcodec.h>
// #include <libavcodec/codec_id.h>
// #include <libavcodec/codec_par.h>
// #include <libavutil/error.h>
// #include <libavutil/mem.h>
// #include <libavutil/pixfmt.h>

// #if __has_include(<rockchip/rk_mpi.h>)
// #include <rockchip/rk_mpi.h>
// #include <rockchip/mpp_buffer.h>
// #include <rockchip/mpp_frame.h>
// #include <rockchip/mpp_packet.h>
// #include <rockchip/mpp_rc_defs.h>
// #elif __has_include(<mpp/rk_mpi.h>)
// #include <mpp/rk_mpi.h>
// #include <mpp/mpp_buffer.h>
// #include <mpp/mpp_frame.h>
// #include <mpp/mpp_packet.h>
// #include <mpp/mpp_rc_defs.h>
// #else
// #include "rk_mpi.h"
// #include "mpp_buffer.h"
// #include "mpp_frame.h"
// #include "mpp_packet.h"
// #include "mpp_rc_defs.h"
// #endif

// #include "Ffmpeg_stream.h"

// #ifndef V4L2_PIX_FMT_NV12
// #define V4L2_PIX_FMT_NV12 0x3231564e
// #endif

// static AVFormatContext *g_ofmt_ctx = NULL;
// static AVStream        *g_st = NULL;

// static MppCtx g_mpp_ctx = NULL;
// static MppApi *g_mpp = NULL;
// static MppEncCfg g_mpp_cfg = NULL;
// static MppBufferGroup g_mpp_buf_grp = NULL;

// static int g_w;
// static int g_h;
// static int g_stride;
// static int g_fps;
// static int64_t g_pts;
// static int64_t g_frame_count;
// static int64_t g_packet_count;

// static void print_av_error(const char *tag, int err)
// {
//     char errbuf[AV_ERROR_MAX_STRING_SIZE];

//     av_strerror(err, errbuf, sizeof(errbuf));
//     printf("%s: %s\n", tag, errbuf);
// }

// static void sig_handler(int sig)
// {
//     printf("signal: %d\n", sig);
//     exit(0);
// }

// static int h264_has_idr(const uint8_t *data, size_t len)
// {
//     size_t i = 0;

//     while (i + 4 < len) {
//         size_t start = 0;

//         if (data[i] == 0x00 && data[i + 1] == 0x00 &&
//             data[i + 2] == 0x01) {
//             start = i + 3;
//         } else if (i + 4 < len &&
//                    data[i] == 0x00 && data[i + 1] == 0x00 &&
//                    data[i + 2] == 0x00 && data[i + 3] == 0x01) {
//             start = i + 4;
//         }

//         if (start) {
//             if ((data[start] & 0x1f) == 5)
//                 return 1;
//             i = start + 1;
//         } else {
//             i++;
//         }
//     }

//     return 0;
// }

// static int write_packet_to_rtmp(MppPacket packet)
// {
//     AVPacket pkt;
//     void *pos;
//     size_t len;
//     int ret;
//     int is_key;

//     if (!packet)
//         return -1;

//     pos = mpp_packet_get_pos(packet);
//     len = mpp_packet_get_length(packet);
//     if (!pos || len <= 0)
//         return 0;

//     av_init_packet(&pkt);
//     pkt.data = NULL;
//     pkt.size = 0;
//     ret = av_new_packet(&pkt, (int)len);
//     if (ret < 0)
//         return -1;

//     memcpy(pkt.data, pos, len);
//     pkt.stream_index = g_st->index;
//     pkt.pts = g_pts - 1;
//     pkt.dts = pkt.pts;
//     pkt.duration = 1;

//     if (h264_has_idr((const uint8_t *)pos, len))
//         pkt.flags |= AV_PKT_FLAG_KEY;
//     is_key = !!(pkt.flags & AV_PKT_FLAG_KEY);

//     av_packet_rescale_ts(&pkt,
//                          (AVRational){1, g_fps},
//                          g_st->time_base);
//     ret = av_interleaved_write_frame(g_ofmt_ctx, &pkt);
//     if (ret < 0)
//         print_av_error("av_interleaved_write_frame failed", ret);
//     else {
//         g_packet_count++;
//         // if ((g_packet_count % 30) == 0)
//         //     printf("RTMP wrote packet count=%lld size=%zu key=%d\n",
//         //            (long long)g_packet_count,
//         //            len,
//         //            is_key);
//     }
//     av_packet_unref(&pkt);

//     if (g_ofmt_ctx && g_ofmt_ctx->pb)
//         avio_flush(g_ofmt_ctx->pb);

//     return ret < 0 ? -1 : 0;
// }

// static int mpp_write_extra_data(void)
// {
//     MppPacket packet = NULL;
//     void *pos;
//     size_t len;
//     int ret;

//     ret = g_mpp->control(g_mpp_ctx, MPP_ENC_GET_EXTRA_INFO, &packet);
//     if (ret || !packet) {
//         printf("MPP_ENC_GET_EXTRA_INFO failed: ret=%d packet=%p\n", ret, packet);
//         return -1;
//     }

//     pos = mpp_packet_get_pos(packet);
//     len = mpp_packet_get_length(packet);
//     if (pos && len > 0) {
//         g_st->codecpar->extradata = av_mallocz(len + AV_INPUT_BUFFER_PADDING_SIZE);
//         if (!g_st->codecpar->extradata) {
//             mpp_packet_deinit(&packet);
//             return -1;
//         }
//         memcpy(g_st->codecpar->extradata, pos, len);
//         g_st->codecpar->extradata_size = (int)len;
//         printf("MPP extra data size: %d\n", g_st->codecpar->extradata_size);
//     } else {
//         printf("MPP extra data is empty\n");
//         mpp_packet_deinit(&packet);
//         return -1;
//     }

//     mpp_packet_deinit(&packet);
//     return 0;
// }

// static int mpp_encoder_init(int w, int h, int stride, int fps)
// {
//     int ret;
//     int bps;

//     ret = mpp_create(&g_mpp_ctx, &g_mpp);
//     if (ret) {
//         printf("mpp_create failed: %d\n", ret);
//         return -1;
//     }

//     ret = mpp_init(g_mpp_ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
//     if (ret) {
//         printf("mpp_init failed: %d\n", ret);
//         return -1;
//     }

//     ret = mpp_enc_cfg_init(&g_mpp_cfg);
//     if (ret)
//         return -1;

//     ret = g_mpp->control(g_mpp_ctx, MPP_ENC_GET_CFG, g_mpp_cfg);
//     if (ret)
//         return -1;

//     bps = 4 * 1000 * 1000;

//     mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:width", w);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:height", h);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:hor_stride", stride);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:ver_stride", h);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:format", MPP_FMT_YUV420SP);

//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:bps_target", bps);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:bps_max", bps * 17 / 16);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:bps_min", bps * 15 / 16);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_in_flex", 0);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_in_num", fps);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_in_denorm", 1);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_out_flex", 0);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_out_num", fps);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_out_denorm", 1);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:gop", fps);

//     mpp_enc_cfg_set_s32(g_mpp_cfg, "codec:type", MPP_VIDEO_CodingAVC);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:profile", 100);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:level", 40);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:cabac_en", 1);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:cabac_idc", 0);
//     mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:trans8x8", 1);

//     ret = g_mpp->control(g_mpp_ctx, MPP_ENC_SET_CFG, g_mpp_cfg);
//     if (ret) {
//         printf("MPP_ENC_SET_CFG failed: %d\n", ret);
//         return -1;
//     }

//     ret = mpp_buffer_group_get_external(&g_mpp_buf_grp, MPP_BUFFER_TYPE_DRM);
//     if (ret) {
//         printf("mpp_buffer_group_get_external failed: %d\n", ret);
//         return -1;
//     }

//     return 0;
// }

// static int rtmp_Init(char *url, int w, int h, int iFmtIn, int fps)
// {
//     int ret;

//     signal(SIGSEGV, sig_handler);
//     signal(SIGABRT, sig_handler);

//     if (iFmtIn != V4L2_PIX_FMT_NV12) {
//         printf("MPP RTMP path expects RGA NV12 DMA input\n");
//         return -1;
//     }

//     g_w = w;
//     g_h = h;
//     g_stride = (w + 15) & ~15;
//     g_fps = fps;
//     g_pts = 0;
//     g_frame_count = 0;
//     g_packet_count = 0;

//     ret = mpp_encoder_init(g_w, g_h, g_stride, g_fps);
//     if (ret)
//         return -1;

//     avformat_network_init();
//     if (avformat_alloc_output_context2(&g_ofmt_ctx, NULL, "flv", url) < 0)
//         return -1;

//     g_ofmt_ctx->flags |= AVFMT_FLAG_FLUSH_PACKETS;
//     g_ofmt_ctx->flush_packets = 1;

//     g_st = avformat_new_stream(g_ofmt_ctx, NULL);
//     if (!g_st)
//         return -1;

//     g_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
//     g_st->codecpar->codec_id = AV_CODEC_ID_H264;
//     g_st->codecpar->width = g_w;
//     g_st->codecpar->height = g_h;
//     g_st->codecpar->format = AV_PIX_FMT_NV12;
//     g_st->time_base = (AVRational){1, g_fps};
//     g_st->avg_frame_rate = (AVRational){g_fps, 1};
//     g_st->r_frame_rate = (AVRational){g_fps, 1};

//     if (mpp_write_extra_data() != 0)
//         return -1;

//     ret = avio_open(&g_ofmt_ctx->pb, url, AVIO_FLAG_WRITE);
//     if (ret < 0) {
//         print_av_error("avio_open failed", ret);
//         return -1;
//     }

//     ret = avformat_write_header(g_ofmt_ctx, NULL);
//     if (ret < 0) {
//         print_av_error("avformat_write_header failed", ret);
//         return -1;
//     }

//     printf("RTMP MPP init OK\n");
//     return 0;
// }

// static int rtmp_PushDma(int dma_fd, int width, int height, int stride, int size)
// {
//     MppBufferInfo info;
//     MppBuffer buffer = NULL;
//     MppFrame frame = NULL;
//     MppPacket packet = NULL;
//     int ret;
//     int retry = 0;

//     if (dma_fd < 0 || !g_mpp)
//         return -1;

//     g_frame_count++;
//     if (g_frame_count <= 10 || (g_frame_count % 30) == 0)
//         // printf("MPP input frame count=%lld fd=%d size=%d stride=%d\n",
//         //        (long long)g_frame_count, dma_fd, size, stride);

//     memset(&info, 0, sizeof(info));
//     info.type = MPP_BUFFER_TYPE_DRM;
//     info.fd = dma_fd;
//     info.size = size;

//     ret = mpp_buffer_import(&buffer, &info);
//     if (ret || !buffer) {
//         printf("mpp_buffer_import failed: %d\n", ret);
//         return -1;
//     }

//     ret = mpp_frame_init(&frame);
//     if (ret) {
//         mpp_buffer_put(buffer);
//         return -1;
//     }

//     mpp_frame_set_width(frame, width);
//     mpp_frame_set_height(frame, height);
//     mpp_frame_set_hor_stride(frame, stride);
//     mpp_frame_set_ver_stride(frame, height);
//     mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
//     mpp_frame_set_pts(frame, g_pts++);
//     mpp_frame_set_buffer(frame, buffer);

//     ret = g_mpp->encode_put_frame(g_mpp_ctx, frame);
//     mpp_frame_deinit(&frame);
//     mpp_buffer_put(buffer);
//     if (ret) {
//         printf("encode_put_frame failed: %d\n", ret);
//         return -1;
//     }

//     do {
//         ret = g_mpp->encode_get_packet(g_mpp_ctx, &packet);
//         if (ret) {
//             printf("encode_get_packet failed: %d\n", ret);
//             return -1;
//         }

//         if (packet) {
//             // if (g_packet_count < 10)
//             //     printf("MPP got packet len=%zu\n",
//             //            mpp_packet_get_length(packet));
//             write_packet_to_rtmp(packet);
//             mpp_packet_deinit(&packet);
//             retry = 0;
//         } else if (retry++ < 5) {
//             usleep(1000);
//         }
//     } while (packet || retry <= 5);

//     return 0;
// }

// static int rtmp_Push(PT_VideoBuf ptBuf)
// {
//     (void)ptBuf;
//     return -1;
// }

// static void rtmp_Exit(void)
// {
//     if (g_ofmt_ctx)
//         av_write_trailer(g_ofmt_ctx);

//     if (g_mpp_cfg)
//         mpp_enc_cfg_deinit(g_mpp_cfg);
//     if (g_mpp_buf_grp)
//         mpp_buffer_group_put(g_mpp_buf_grp);
//     if (g_mpp_ctx)
//         mpp_destroy(g_mpp_ctx);

//     if (g_ofmt_ctx && g_ofmt_ctx->pb)
//         avio_closep(&g_ofmt_ctx->pb);
//     if (g_ofmt_ctx)
//         avformat_free_context(g_ofmt_ctx);
// }

// static int rtmp_isSupport(int iFmtIn, int type)
// {
//     return (type == FFMPEG_STREAM_RTMP);
// }

// static T_FfmpegStream g_rtmpStream = {
//     .name = "rtmp_stream",
//     .iProtocol = FFMPEG_STREAM_RTMP,
//     .isSupport = rtmp_isSupport,
//     .Init = rtmp_Init,
//     .Push = rtmp_Push,
//     .PushDma = rtmp_PushDma,
//     .Exit = rtmp_Exit,
// };

// int RtmpStreamRegister(void)
// {
//     return RegisterFfmpegStream(&g_rtmpStream);
// }






#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>

#if __has_include(<rockchip/rk_mpi.h>)
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_rc_defs.h>
#elif __has_include(<mpp/rk_mpi.h>)
#include <mpp/rk_mpi.h>
#include <mpp/mpp_buffer.h>
#include <mpp/mpp_frame.h>
#include <mpp/mpp_packet.h>
#include <mpp/mpp_rc_defs.h>
#else
#include "rk_mpi.h"
#include "mpp_buffer.h"
#include "mpp_frame.h"
#include "mpp_packet.h"
#include "mpp_rc_defs.h"
#endif

#include "Ffmpeg_stream.h"

#ifndef V4L2_PIX_FMT_NV12
#define V4L2_PIX_FMT_NV12 0x3231564e
#endif

static AVFormatContext *g_ofmt_ctx = NULL;
static AVStream        *g_st = NULL;

static MppCtx g_mpp_ctx = NULL;
static MppApi *g_mpp = NULL;
static MppEncCfg g_mpp_cfg = NULL;
static MppBufferGroup g_mpp_buf_grp = NULL;

static int g_w;
static int g_h;
static int g_stride;
static int g_fps;
static int64_t g_start_ms;
static int64_t g_last_packet_pts;
static int64_t g_frame_count;
static int64_t g_packet_count;

static int64_t get_monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int64_t get_stream_pts_ms(void)
{
    int64_t pts = get_monotonic_ms() - g_start_ms;

    if (pts <= g_last_packet_pts)
        pts = g_last_packet_pts + 1;

    g_last_packet_pts = pts;
    return pts;
}

static void print_av_error(const char *tag, int err)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];

    av_strerror(err, errbuf, sizeof(errbuf));
    printf("%s: %s\n", tag, errbuf);
}

static void sig_handler(int sig)
{
    printf("signal: %d\n", sig);
    exit(0);
}

static int h264_has_idr(const uint8_t *data, size_t len)
{
    size_t i = 0;

    while (i + 4 < len) {
        size_t start = 0;

        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x01) {
            start = i + 3;
        } else if (i + 4 < len &&
                   data[i] == 0x00 && data[i + 1] == 0x00 &&
                   data[i + 2] == 0x00 && data[i + 3] == 0x01) {
            start = i + 4;
        }

        if (start) {
            if ((data[start] & 0x1f) == 5)
                return 1;
            i = start + 1;
        } else {
            i++;
        }
    }

    return 0;
}

static int write_packet_to_rtmp(MppPacket packet)
{
    AVPacket pkt;
    void *pos;
    size_t len;
    int ret;
    int is_key;

    if (!packet)
        return -1;

    pos = mpp_packet_get_pos(packet);
    len = mpp_packet_get_length(packet);
    if (!pos || len <= 0)
        return 0;

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    ret = av_new_packet(&pkt, (int)len);
    if (ret < 0)
        return -1;

    memcpy(pkt.data, pos, len);
    pkt.stream_index = g_st->index;
    pkt.pts = get_stream_pts_ms();
    pkt.dts = pkt.pts;
    pkt.duration = 1000 / g_fps;

    if (h264_has_idr((const uint8_t *)pos, len))
        pkt.flags |= AV_PKT_FLAG_KEY;
    is_key = !!(pkt.flags & AV_PKT_FLAG_KEY);

    av_packet_rescale_ts(&pkt,
                         (AVRational){1, 1000},
                         g_st->time_base);
    ret = av_write_frame(g_ofmt_ctx, &pkt);
    if (ret < 0)
        print_av_error("av_write_frame failed", ret);
    else {
        g_packet_count++;
        // if ((g_packet_count % 30) == 0)
        //     printf("RTMP wrote packet count=%lld size=%zu key=%d\n",
        //            (long long)g_packet_count,
        //            len,
        //            is_key);
    }
    av_packet_unref(&pkt);

    if (g_ofmt_ctx && g_ofmt_ctx->pb)
        avio_flush(g_ofmt_ctx->pb);

    return ret < 0 ? -1 : 0;
}

static int mpp_write_extra_data(void)
{
    MppPacket packet = NULL;
    void *pos;
    size_t len;
    int ret;

    ret = g_mpp->control(g_mpp_ctx, MPP_ENC_GET_EXTRA_INFO, &packet);
    if (ret || !packet) {
        printf("MPP_ENC_GET_EXTRA_INFO failed: ret=%d packet=%p\n", ret, packet);
        return -1;
    }

    pos = mpp_packet_get_pos(packet);
    len = mpp_packet_get_length(packet);
    if (pos && len > 0) {
        g_st->codecpar->extradata = av_mallocz(len + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!g_st->codecpar->extradata) {
            mpp_packet_deinit(&packet);
            return -1;
        }
        memcpy(g_st->codecpar->extradata, pos, len);
        g_st->codecpar->extradata_size = (int)len;
        printf("MPP extra data size: %d\n", g_st->codecpar->extradata_size);
    } else {
        printf("MPP extra data is empty\n");
        mpp_packet_deinit(&packet);
        return -1;
    }

    mpp_packet_deinit(&packet);
    return 0;
}

static int mpp_encoder_init(int w, int h, int stride, int fps)
{
    int ret;
    int bps;

    ret = mpp_create(&g_mpp_ctx, &g_mpp);
    if (ret) {
        printf("mpp_create failed: %d\n", ret);
        return -1;
    }

    ret = mpp_init(g_mpp_ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
    if (ret) {
        printf("mpp_init failed: %d\n", ret);
        return -1;
    }

    ret = mpp_enc_cfg_init(&g_mpp_cfg);
    if (ret)
        return -1;

    ret = g_mpp->control(g_mpp_ctx, MPP_ENC_GET_CFG, g_mpp_cfg);
    if (ret)
        return -1;

    bps = 2 * 1000 * 1000;

    mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:width", w);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:height", h);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:hor_stride", stride);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:ver_stride", h);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "prep:format", MPP_FMT_YUV420SP);

    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:bps_target", bps);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:bps_max", bps * 17 / 16);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:bps_min", bps * 15 / 16);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_in_num", fps);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_out_flex", 0);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_out_num", fps);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:fps_out_denorm", 1);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "rc:gop", fps);

    mpp_enc_cfg_set_s32(g_mpp_cfg, "codec:type", MPP_VIDEO_CodingAVC);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:profile", 100);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:level", 40);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:cabac_en", 1);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:cabac_idc", 0);
    mpp_enc_cfg_set_s32(g_mpp_cfg, "h264:trans8x8", 1);

    ret = g_mpp->control(g_mpp_ctx, MPP_ENC_SET_CFG, g_mpp_cfg);
    if (ret) {
        printf("MPP_ENC_SET_CFG failed: %d\n", ret);
        return -1;
    }

    ret = mpp_buffer_group_get_external(&g_mpp_buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        printf("mpp_buffer_group_get_external failed: %d\n", ret);
        return -1;
    }

    return 0;
}

static int rtmp_Init(char *url, int w, int h, int iFmtIn, int fps)
{
    int ret;

    signal(SIGSEGV, sig_handler);
    signal(SIGABRT, sig_handler);

    if (iFmtIn != V4L2_PIX_FMT_NV12) {
        printf("MPP RTMP path expects RGA NV12 DMA input\n");
        return -1;
    }

    g_w = w;
    g_h = h;
    g_stride = (w + 15) & ~15;
    g_fps = fps;
    g_start_ms = get_monotonic_ms();
    g_last_packet_pts = -1;
    g_frame_count = 0;
    g_packet_count = 0;

    ret = mpp_encoder_init(g_w, g_h, g_stride, g_fps);
    if (ret)
        return -1;

    avformat_network_init();
    if (avformat_alloc_output_context2(&g_ofmt_ctx, NULL, "flv", url) < 0)
        return -1;

    g_ofmt_ctx->flags |= AVFMT_FLAG_FLUSH_PACKETS;
    g_ofmt_ctx->flush_packets = 1;
    g_ofmt_ctx->max_delay = 0;

    g_st = avformat_new_stream(g_ofmt_ctx, NULL);
    if (!g_st)
        return -1;

    g_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    g_st->codecpar->codec_id = AV_CODEC_ID_H264;
    g_st->codecpar->width = g_w;
    g_st->codecpar->height = g_h;
    g_st->codecpar->format = AV_PIX_FMT_NV12;
    g_st->time_base = (AVRational){1, 1000};
    g_st->avg_frame_rate = (AVRational){g_fps, 1};
    g_st->r_frame_rate = (AVRational){g_fps, 1};

    if (mpp_write_extra_data() != 0)
        return -1;

    ret = avio_open(&g_ofmt_ctx->pb, url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        print_av_error("avio_open failed", ret);
        return -1;
    }

    ret = avformat_write_header(g_ofmt_ctx, NULL);
    if (ret < 0) {
        print_av_error("avformat_write_header failed", ret);
        return -1;
    }

    printf("RTMP MPP init OK\n");
    return 0;
}

static int rtmp_PushDma(int dma_fd, int width, int height, int stride, int size)
{
    MppBufferInfo info;
    MppBuffer buffer = NULL;
    MppFrame frame = NULL;
    MppPacket packet = NULL;
    int ret;
    int retry = 0;

    if (dma_fd < 0 || !g_mpp)
        return -1;

    g_frame_count++;
    // if (g_frame_count <= 10 || (g_frame_count % 30) == 0)
    //     printf("MPP input frame count=%lld fd=%d size=%d stride=%d\n",
    //            (long long)g_frame_count, dma_fd, size, stride);

    memset(&info, 0, sizeof(info));
    info.type = MPP_BUFFER_TYPE_DRM;
    info.fd = dma_fd;
    info.size = size;

    ret = mpp_buffer_import(&buffer, &info);
    if (ret || !buffer) {
        printf("mpp_buffer_import failed: %d\n", ret);
        return -1;
    }

    ret = mpp_frame_init(&frame);
    if (ret) {
        mpp_buffer_put(buffer);
        return -1;
    }

    mpp_frame_set_width(frame, width);
    mpp_frame_set_height(frame, height);
    mpp_frame_set_hor_stride(frame, stride);
    mpp_frame_set_ver_stride(frame, height);
    mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
    mpp_frame_set_pts(frame, get_monotonic_ms() - g_start_ms);
    mpp_frame_set_buffer(frame, buffer);

    ret = g_mpp->encode_put_frame(g_mpp_ctx, frame);
    mpp_frame_deinit(&frame);
    mpp_buffer_put(buffer);
    if (ret) {
        printf("encode_put_frame failed: %d\n", ret);
        return -1;
    }

    do {
        ret = g_mpp->encode_get_packet(g_mpp_ctx, &packet);
        if (ret) {
            printf("encode_get_packet failed: %d\n", ret);
            return -1;
        }

        if (packet) {
            if (g_packet_count < 10)
                printf("MPP got packet len=%zu\n",
                       mpp_packet_get_length(packet));
            write_packet_to_rtmp(packet);
            mpp_packet_deinit(&packet);
            retry = 0;
        } else if (retry++ < 5) {
            usleep(1000);
        }
    } while (packet || retry <= 5);

    return 0;
}

static int rtmp_Push(PT_VideoBuf ptBuf)
{
    (void)ptBuf;
    return -1;
}

static void rtmp_Exit(void)
{
    if (g_ofmt_ctx)
        av_write_trailer(g_ofmt_ctx);

    if (g_mpp_cfg)
        mpp_enc_cfg_deinit(g_mpp_cfg);
    if (g_mpp_buf_grp)
        mpp_buffer_group_put(g_mpp_buf_grp);
    if (g_mpp_ctx)
        mpp_destroy(g_mpp_ctx);

    if (g_ofmt_ctx && g_ofmt_ctx->pb)
        avio_closep(&g_ofmt_ctx->pb);
    if (g_ofmt_ctx)
        avformat_free_context(g_ofmt_ctx);
}

static int rtmp_isSupport(int iFmtIn, int type)
{
    return (type == FFMPEG_STREAM_RTMP);
}

static T_FfmpegStream g_rtmpStream = {
    .name = "rtmp_stream",
    .iProtocol = FFMPEG_STREAM_RTMP,
    .isSupport = rtmp_isSupport,
    .Init = rtmp_Init,
    .Push = rtmp_Push,
    .PushDma = rtmp_PushDma,
    .Exit = rtmp_Exit,
};

int RtmpStreamRegister(void)
{
    return RegisterFfmpegStream(&g_rtmpStream);
}
