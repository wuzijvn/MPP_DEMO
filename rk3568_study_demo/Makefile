# ========= 工具链 =========
CXX := aarch64-linux-gnu-g++

# ========= 目录 =========
FFMPEG_PREFIX := /opt/rockchip/ffmpeg-rockchip
MPP_PREFIX    := /opt/rockchip/rkmpp-cross
RGA_PREFIX    := /opt/rockchip/rkrga-cross
DRM_PREFIX    := /opt/rockchip/rklibdrm-cross

# ========= 编译 / 链接参数 =========
CXXFLAGS := \
    -I$(FFMPEG_PREFIX)/include \
    -I$(MPP_PREFIX)/include \
    -I$(RGA_PREFIX)/include \
    -I$(DRM_PREFIX)/include \
    -O2 -g

LDFLAGS := \
    -L$(FFMPEG_PREFIX)/lib \
    -L$(MPP_PREFIX)/lib \
    -L$(RGA_PREFIX)/lib \
    -L$(DRM_PREFIX)/lib

LIBS := \
    -lavformat -lavcodec -lavutil \
    -lswscale -lswresample \
    -lrockchip_mpp -lrga -ldrm \
    -lpthread -ldl

# ========= 目标 =========
TARGET := demo

all: $(TARGET)

$(TARGET): demo.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
