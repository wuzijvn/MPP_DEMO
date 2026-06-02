cmake_minimum_required(VERSION 3.10)
project(rk3568_demo CXX)

# ========= 交叉编译设置 =========
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# ========= SDK 路径 =========
set(FFMPEG_PREFIX /opt/rockchip/ffmpeg-rockchip)
set(MPP_PREFIX    /opt/rockchip/rkmpp-cross)
set(RGA_PREFIX    /opt/rockchicd p/rkrga-cross)
set(DRM_PREFIX    /opt/rockchip/rklibdrm-cross)

# ========= 头文件 =========
include_directories(
    ${FFMPEG_PREFIX}/include
    ${MPP_PREFIX}/include
    ${RGA_PREFIX}/include
    ${DRM_PREFIX}/include
)

# ========= 库路径 =========
link_directories(
    ${FFMPEG_PREFIX}/lib
    ${MPP_PREFIX}/lib
    ${RGA_PREFIX}/lib
    ${DRM_PREFIX}/lib
)

# ========= 可执行文件 =========
add_executable(demo demo.cpp)

# ========= 链接库 =========
target_link_libraries(demo
    avformat
    avcodec
    avutil
    swscale
    swresample
    rockchip_mpp
    rga
    drm
    pthread
    dl
)
