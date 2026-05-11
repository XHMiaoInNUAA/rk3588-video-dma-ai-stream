# # 注释掉交叉编译！！！
# # CROSS_COMPILE = aarch64-linux-gnu-

# AS		= $(CROSS_COMPILE)as
# LD		= $(CROSS_COMPILE)ld
# CC		= $(CROSS_COMPILE)gcc
# CPP		= $(CC) -E
# AR		= $(CROSS_COMPILE)ar
# NM		= $(CROSS_COMPILE)nm

# STRIP		= $(CROSS_COMPILE)strip
# OBJCOPY		= $(CROSS_COMPILE)objcopy
# OBJDUMP		= $(CROSS_COMPILE)objdump

# export AS LD CC CPP AR NM
# export STRIP OBJCOPY OBJDUMP

# CFLAGS := -Wall -O2 -g
# CFLAGS += -I $(shell pwd)/include
# CFLAGS += -I $(shell pwd)/FFmpeg
# CFLAGS += -I/usr/local/include

# LDFLAGS := -L/usr/local/lib
# -Wl,-rpath=/usr/local/lib
# LDFLAGS := -lm -ljpeg -lrga -pthread -lavformat -lavcodec -lavutil -lswscale

# export CFLAGS LDFLAGS

# TOPDIR := $(shell pwd)
# export TOPDIR

# TARGET := video2lcd

# obj-y += main.o
# obj-y += convert/
# obj-y += display/
# obj-y += render/
# obj-y += video/
# obj-y += FFmpeg/

# all : start_recursive_build $(TARGET)
# 	@echo $(TARGET) has been built!

# start_recursive_build:
# 	make -C ./ -f $(TOPDIR)/Makefile.build

# $(TARGET) : start_recursive_build
# 	$(CC) -o $(TARGET) built-in.o $(LDFLAGS)

# clean:
# 	rm -f $(shell find -name "*.o")
# 	rm -f $(TARGET)

# distclean:
# 	rm -f $(shell find -name "*.o")
# 	rm -f $(shell find -name "*.d")
# 	rm -f $(TARGET)

# 取消交叉编译
# CROSS_COMPILE = aarch64-linux-gnu-

AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
CC      = $(CROSS_COMPILE)gcc
CPP     = $(CC) -E
AR      = $(CROSS_COMPILE)ar
NM      = $(CROSS_COMPILE)nm

STRIP   = $(CROSS_COMPILE)strip
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

CXX = $(CROSS_COMPILE)g++
export AS LD CC CXX CPP AR NM
export STRIP OBJCOPY OBJDUMP

# ===============================
# pkg-config
# ===============================
PKG_CONFIG := PKG_CONFIG_PATH=/usr/local/ffmpeg/lib/pkgconfig pkg-config
RKLLM_EXAMPLE_DIR ?= /home/orangepi/rknn-llm/examples/multimodal_model_demo
RKLLM_DEPLOY_DIR ?= $(RKLLM_EXAMPLE_DIR)/deploy
TARGET_LIB_ARCH ?= aarch64
RKNN_API_DIR ?= $(RKLLM_DEPLOY_DIR)/3rdparty/librknnrt/Linux/librknn_api
RKLLM_API_DIR ?= /home/orangepi/rknn-llm/rkllm-runtime/Linux/librkllm_api
OPENCV_ROOT ?= $(RKLLM_DEPLOY_DIR)/3rdparty/opencv/opencv-linux-aarch64
# ================================
# 编译参数
# ===============================
CFLAGS := -Wall -O2 -g
CFLAGS += -I$(shell pwd)/include
CFLAGS += -I$(shell pwd)/FFmpeg

# CFLAGS += -I$(RKLLM_EXAMPLE_DIR)
# CFLAGS += -I$(RKLLM_EXAMPLE_DIR)/src
# CFLAGS += -I$(RKLLM_EXAMPLE_DIR)/include
# CFLAGS += -I$(RKLLM_EXAMPLE_DIR)/cpp
# CFLAGS += -I$(RKLLM_EXAMPLE_DIR)/common
# CFLAGS += -I$(RKLLM_EXAMPLE_DIR)/deploy/src
# CFLAGS += -I$(RKLLM_DEMO_DIR)
# CFLAGS += -I$(RKLLM_DEMO_DIR)/include
# CFLAGS += -I$(RKNN_API_DIR)/include
CFLAGS := -Wall -O2 -g
CFLAGS += -I$(shell pwd)/include
CFLAGS += -I$(shell pwd)/FFmpeg
CFLAGS += -I$(shell pwd)/infer
CFLAGS += -I$(RKLLM_DEPLOY_DIR)/src
CFLAGS += -I$(RKNN_API_DIR)/include
CFLAGS += -I$(RKLLM_API_DIR)/include
CFLAGS += -I$(OPENCV_ROOT)/include
CFLAGS += -I$(OPENCV_ROOT)/include/opencv4
CFLAGS += $(shell $(PKG_CONFIG) --cflags libavformat libavcodec libavutil libswscale)
CFLAGS += $(shell pkg-config --cflags libdrm)
CFLAGS += $(shell pkg-config --cflags rockchip_mpp 2>/dev/null)
CFLAGS += -I/usr/include/rockchip -I/usr/include/mpp -I/usr/local/include/rockchip -I/usr/local/include/mpp

CXXFLAGS := $(CFLAGS) -std=c++11
CXXFLAGS += $(shell pkg-config --cflags opencv4 2>/dev/null)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libavformat libavcodec libavutil libswscale)

# ===============================
# 链接参数（分开写，保证顺序）
# ===============================
FFMPEG_LIBS := $(shell $(PKG_CONFIG) --libs libavformat libavcodec libavutil libswscale)
OPENCV_LIBS := -L$(OPENCV_ROOT)/lib -Wl,-rpath=$(OPENCV_ROOT)/lib -lopencv_imgcodecs -lopencv_imgproc -lopencv_core
# SYS_LIBS := -lrga -ldrm -pthread -lm -ljpeg
RKNN_LIB := $(RKNN_API_DIR)/$(TARGET_LIB_ARCH)/librknnrt.so
RKLLM_LIB := $(RKLLM_API_DIR)/$(TARGET_LIB_ARCH)/librkllmrt.so
RKLLM_LIBS ?= $(RKNN_LIB) $(RKLLM_LIB)
SYS_LIBS := -lrga -ldrm -pthread -lm -ljpeg -lrockchip_mpp
# $(CC) ... $(FFMPEG_LIBS) $(SYS_LIBS)


LDFLAGS := -L/usr/local/ffmpeg/lib -Wl,-rpath=/usr/local/ffmpeg/lib -Wl,--allow-shlib-undefined -Wl,-Bsymbolic


export CFLAGS CXXFLAGS LDFLAGS

TOPDIR := $(shell pwd)
export TOPDIR

TARGET := video2lcd

obj-y += main.o
obj-y += dmabuf/
obj-y += convert/
obj-y += display/
obj-y += render/
obj-y += video/
obj-y += FFmpeg/
# Enable this only after streaming is stable and INFER_ENABLE is set to 1.
obj-y += infer/

# ===============================
# 构建
# ===============================
all : start_recursive_build $(TARGET)
	@echo "==== $(TARGET) has been built! ===="

start_recursive_build:
	$(MAKE) -C ./ -f $(TOPDIR)/Makefile.build

$(TARGET) : start_recursive_build
	$(CXX) -o $(TARGET) built-in.o $(LDFLAGS) $(FFMPEG_LIBS) $(RKLLM_LIBS) $(OPENCV_LIBS) $(SYS_LIBS)

# ===============================
# 清理
# ===============================
clean:
	rm -f $(shell find -name "*.o")
	rm -f $(TARGET)

distclean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.d")
	rm -f $(TARGET)