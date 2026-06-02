#ifndef STAGE01_V4L2_IMAGE_HPP_
#define STAGE01_V4L2_IMAGE_HPP_

#include <stdio.h>

#include <string>
#include <vector>

namespace stage01_v4l2 {

/*
===============================================================================
Stage01 Image Conversion Utilities
===============================================================================

为什么要有这个文件：
1) 采集出来的原始数据（例如 YUYV）人眼无法直接查看；
2) 我们需要快速把一帧转成可视化图片判断“采集是否正常”；
3) 为了避免引入 OpenCV/FFmpeg 依赖，选择最简单的 PPM 格式。

学习重点：
1) 理解 YUYV 的内存布局（2像素共享 U/V）；
2) 理解 stride(bytesperline) 与紧凑布局差异；
3) 理解“显示正确”不代表“链路全正确”，但它是第一道验证。
*/

// clamp_u8:
//   把 int 限制在 [0, 255]，用于颜色计算防溢出。
inline unsigned char clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

// yuyv_to_rgb24:
//   把 YUYV422 转成 RGB24。
//
// YUYV 字节布局（每4字节=2像素）：
//   Byte0=Y0, Byte1=U, Byte2=Y1, Byte3=V
//   即两个相邻像素共享一组 U/V。
//
// 这里采用常见 BT.601 整数近似公式，避免浮点开销。
//
// 注意：
//   本函数按“紧凑布局”读取（每行 width*2 字节）。
//   若驱动给的 bytesperline > width*2，需要按 stride 改为逐行读取。
inline void yuyv_to_rgb24(const unsigned char* yuyv,
                          int width,
                          int height,
                          std::vector<unsigned char>* rgb) {
    // RGB24 每像素 3 字节，因此总大小是 width*height*3。
    rgb->assign((size_t)width * (size_t)height * 3U, 0);
    size_t j = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 2) {
            // YUYV 紧凑布局下，每像素平均占 2 字节。
            size_t base = ((size_t)y * (size_t)width + (size_t)x) * 2U;
            int y0 = yuyv[base + 0];
            int u = yuyv[base + 1];
            int y1 = yuyv[base + 2];
            int v = yuyv[base + 3];
            // 注意 U/V 被一对像素共享，这也是 4:2:2 的核心特征。

            // BT.601 常见整数近似：
            // 先把 YUV 转换前做偏移校正。
            int c0 = y0 - 16;
            int c1 = y1 - 16;
            int d = u - 128;
            int e = v - 128;
            if (c0 < 0) c0 = 0;
            if (c1 < 0) c1 = 0;

            int r0 = (298 * c0 + 409 * e + 128) >> 8;
            int g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
            int b0 = (298 * c0 + 516 * d + 128) >> 8;

            int r1 = (298 * c1 + 409 * e + 128) >> 8;
            int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
            int b1 = (298 * c1 + 516 * d + 128) >> 8;

            // 两个像素依次写入 RGB 缓冲。
            (*rgb)[j++] = clamp_u8(r0);
            (*rgb)[j++] = clamp_u8(g0);
            (*rgb)[j++] = clamp_u8(b0);
            (*rgb)[j++] = clamp_u8(r1);
            (*rgb)[j++] = clamp_u8(g1);
            (*rgb)[j++] = clamp_u8(b1);
        }
    }
}

// save_ppm_from_yuyv:
//   输入一帧 YUYV，转换并保存为 PPM(P6)。
//
// 为什么是 PPM：
// 1) 格式极简（文本头 + RGB 二进制体）；
// 2) 不依赖 OpenCV/FFmpeg，也能快速肉眼验证图像。
inline bool save_ppm_from_yuyv(const char* ppm_path,
                               const void* yuyv_data,
                               size_t yuyv_bytes,
                               int width,
                               int height) {
    // 紧凑 YUYV 理论最小字节数：width*height*2。
    size_t need = (size_t)width * (size_t)height * 2U;
    if (yuyv_bytes < need) {
        fprintf(stderr, "save_ppm_from_yuyv: yuyv bytes too small: got=%zu need=%zu\n", yuyv_bytes, need);
        return false;
    }

    // 先做颜色空间转换，再一次性写盘。
    std::vector<unsigned char> rgb;
    yuyv_to_rgb24((const unsigned char*)yuyv_data, width, height, &rgb);

    FILE* fp = fopen(ppm_path, "wb");
    if (!fp) {
        perror("fopen ppm output");
        return false;
    }

    // PPM P6 头格式（无压缩二进制 RGB）：
    // P6
    // <width> <height>
    // 255
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    // 紧接着写入原始 RGB24 字节流，无额外压缩。
    size_t wrote = fwrite(rgb.data(), 1, rgb.size(), fp);
    fclose(fp);
    if (wrote != rgb.size()) {
        fprintf(stderr, "write ppm failed: %zu/%zu\n", wrote, rgb.size());
        return false;
    }
    return true;
}

// save_ppm_from_yuyv_with_stride:
//   支持 stride(bytesperline) 的 YUYV -> PPM 导出。
//
// 适用场景：
//   某些驱动会把每行对齐到更大字节数（bytesperline > width*2），
//   这时不能简单按紧凑布局转换，否则会出现颜色错位或画面倾斜。
inline bool save_ppm_from_yuyv_with_stride(const char* ppm_path,
                                           const void* yuyv_data,
                                           size_t yuyv_bytes,
                                           int width,
                                           int height,
                                           int bytesperline) {
    if (width <= 0 || height <= 0 || bytesperline <= 0) {
        fprintf(stderr, "save_ppm_from_yuyv_with_stride: invalid geometry\n");
        return false;
    }
    if ((width % 2) != 0) {
        fprintf(stderr, "save_ppm_from_yuyv_with_stride: width must be even for YUYV\n");
        return false;
    }

    // min_bpl 是“每行最小理论字节”，实际 bytesperline 可大于它（对齐导致）。
    const size_t min_bpl = (size_t)width * 2U;
    if ((size_t)bytesperline < min_bpl) {
        fprintf(stderr,
                "save_ppm_from_yuyv_with_stride: bytesperline too small: %d < %zu\n",
                bytesperline,
                min_bpl);
        return false;
    }

    // 这里按 stride 计算最小可读总字节，不能再用 width*height*2。
    const size_t need = (size_t)bytesperline * (size_t)height;
    if (yuyv_bytes < need) {
        fprintf(stderr,
                "save_ppm_from_yuyv_with_stride: yuyv bytes too small: got=%zu need=%zu\n",
                yuyv_bytes,
                need);
        return false;
    }

    const unsigned char* src = (const unsigned char*)yuyv_data;
    std::vector<unsigned char> rgb((size_t)width * (size_t)height * 3U, 0);
    size_t out = 0;

    for (int y = 0; y < height; ++y) {
        // 每行起点按 bytesperline 跨越，这就是 stride 感知的核心。
        const unsigned char* row = src + (size_t)y * (size_t)bytesperline;
        for (int x = 0; x < width; x += 2) {
            // 行内仍按 YUYV 双像素分组解析。
            size_t base = (size_t)x * 2U;
            int y0 = row[base + 0];
            int u = row[base + 1];
            int y1 = row[base + 2];
            int v = row[base + 3];

            int c0 = y0 - 16;
            int c1 = y1 - 16;
            int d = u - 128;
            int e = v - 128;
            if (c0 < 0) c0 = 0;
            if (c1 < 0) c1 = 0;

            int r0 = (298 * c0 + 409 * e + 128) >> 8;
            int g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
            int b0 = (298 * c0 + 516 * d + 128) >> 8;
            int r1 = (298 * c1 + 409 * e + 128) >> 8;
            int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
            int b1 = (298 * c1 + 516 * d + 128) >> 8;

            rgb[out++] = clamp_u8(r0);
            rgb[out++] = clamp_u8(g0);
            rgb[out++] = clamp_u8(b0);
            rgb[out++] = clamp_u8(r1);
            rgb[out++] = clamp_u8(g1);
            rgb[out++] = clamp_u8(b1);
        }
    }

    FILE* fp = fopen(ppm_path, "wb");
    if (!fp) {
        perror("fopen ppm output");
        return false;
    }
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    size_t wrote = fwrite(rgb.data(), 1, rgb.size(), fp);
    fclose(fp);
    if (wrote != rgb.size()) {
        fprintf(stderr, "write ppm failed: %zu/%zu\n", wrote, rgb.size());
        return false;
    }
    return true;
}

}  // namespace stage01_v4l2

#endif  // STAGE01_V4L2_IMAGE_HPP_
