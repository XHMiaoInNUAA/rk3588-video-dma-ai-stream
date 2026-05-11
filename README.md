# RK3588 Video DMA Pipeline

Real-time RK3588 video pipeline with V4L2 capture, RGA DMA-BUF conversion, MPP RTMP streaming, and RKNN/RKLLM inference.

## Overview

This project captures camera frames through V4L2, converts them with Rockchip RGA, exports the converted frames as DMA-BUF buffers, and sends them to two independent paths:

- **Streaming path**: `YUYV -> RGA -> NV12 DMA-BUF -> MPP H.264 encoder -> RTMP`
- **Inference path**: `YUYV -> RGA -> RGB888 DMA-BUF -> frame-diff gate -> image encoder -> RKLLM`

The camera input does not need DMA capture support. DMA-BUF starts after the RGA conversion stage.

## Pipeline

```text
V4L2 Camera YUYV
      |
      v
Main Capture Thread
      |
      v
RGA Thread
      |-------------------------------|
      |                               |
      v                               v
YUYV -> NV12 DMA-BUF            YUYV -> RGB888 DMA-BUF
      |                               |
      v                               v
Push Thread                      Infer Thread
      |                               |
      v                               v
MPP H.264 Encoder                Frame Difference Gate
      |                               |
      v                               v
RTMP Stream                      RKNN Image Encoder
                                      |
                                      v
                              8-frame RKLLM Analysis
```

## Features

- V4L2 camera capture
- RGA hardware color conversion
- DMA-BUF buffer pool based on Linux DMA heap
- MPP hardware H.264 encoding
- RTMP streaming through FFmpeg muxing
- Optional RKNN/RKLLM multimodal inference
- Frame difference gate to reduce unnecessary model calls
- Three-thread runtime design for capture/RGA, streaming, and inference

## Directory Structure

```text
.
├── main.c                 # Main runtime pipeline and thread orchestration
├── dmabuf/                # DMA-BUF allocation and buffer pool manager
├── video/                 # V4L2 camera capture
├── FFmpeg/                # RTMP/MPP streaming backend
├── infer/                 # RGB inference bridge, frame diff, RKNN/RKLLM glue
├── include/               # Public headers and configuration
├── Makefile
└── Makefile.build
```

Legacy folders such as `convert/`, `display/`, and `render/` are not used by the current DMA streaming/inference pipeline.

## Requirements

Target platform:

- RK3588 / RK3588S board, such as Orange Pi 5/5B
- Linux with V4L2 camera support
- Rockchip RGA library
- Rockchip MPP library
- FFmpeg development libraries
- RKNN runtime and RKLLM runtime, if inference is enabled
- OpenCV runtime from the RKLLM multimodal demo, if inference is enabled

The Makefile currently assumes RKLLM demo paths similar to:

```text
/home/orangepi/rknn-llm/examples/multimodal_model_demo
/home/orangepi/rknn-llm/rkllm-runtime
```

Adjust these variables in `Makefile` if your board uses different paths:

```makefile
RKLLM_EXAMPLE_DIR
RKLLM_DEPLOY_DIR
RKNN_API_DIR
RKLLM_API_DIR
OPENCV_ROOT
```

## Configuration

Main configuration is in `include/config.h`.

Camera capture size:

```c
#define VIDEO_CAPTURE_WIDTH  640
#define VIDEO_CAPTURE_HEIGHT 480
```

Enable or disable inference:

```c
#define INFER_ENABLE 1
```

Set model paths:

```c
#define INFER_ENCODER_MODEL_PATH "/path/to/qwen3-vl_vision_rk3588.rknn"
#define INFER_LLM_MODEL_PATH     "/path/to/qwen3-vl-2b-instruct_w8a8_rk3588.rkllm"
```

Frame difference gate:

```c
#define INFER_FRAME_DIFF_ENABLE  1
#define INFER_FRAME_DIFF_PIXEL_THRESHOLD 45
#define INFER_FRAME_DIFF_RATIO_THRESHOLD 0.03f
```

With the default settings, a sampled RGB frame is sent to the image encoder only when at least about 3% of pixels change significantly compared with the previous sampled frame.

## Build

On the RK3588 board:

```bash
make distclean
make
```

The output binary is:

```text
video2lcd
```

Despite the historical name, the current active pipeline is for video streaming and inference. LCD display code is not used.

## Run

Start the pipeline:

```bash
./video2lcd /dev/video0
```

The RTMP URL is currently configured in `main.c`:

```c
FfmpegStreamInit("rtmp://192.168.31.153:1935/live/test", ...);
```

Use a low-latency player command such as:

```bash
ffplay -fflags nobuffer -flags low_delay -framedrop rtmp://192.168.31.153:1935/live/test
```

## Runtime Notes

The inference path samples RGB frames about every 500 ms. It does not run inference for every video frame.

Inference flow:

```text
RGB888 sampled frame
  -> frame difference check
  -> run_imgenc
  -> collect 8 embeddings
  -> rkllm_run
```

If RKLLM is busy answering, new image encoder work may be skipped to avoid RKNN/RKLLM runtime conflicts.

Typical logs:

```text
FrameDiff pass frame=140 changed=2800 ratio=0.0558
FrameDiff drop frame=155 changed=320 ratio=0.0064 drop=1
LLM busy, skip imgenc frame=7185 drop=420
```

## Troubleshooting

If streaming works when inference is disabled but fails when inference is enabled, first test with:

```c
#define INFER_ENABLE 0
```

If pure streaming is stable, the issue is likely caused by RKNN/RKLLM resource pressure or NPU runtime conflicts.

If FFmpeg/RTMP latency grows over time, check:

- RTMP server buffering
- `ffplay` low-latency options
- packet timestamps in `FFmpeg/rtmp_stream.c`
- whether `av_write_frame()` is blocked by network output

If model headers or libraries are not found, verify paths in `Makefile` against the RKLLM multimodal demo `CMakeLists.txt`.

## License

Add a license file before publishing if you plan to make the repository public.
