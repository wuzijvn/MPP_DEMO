#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/*
Week4 主题: 拷贝路径 vs 零拷贝路径（概念模拟）

这个文件不是硬件真实零拷贝实现，而是“成本模型”演示：
1) COPY_PATH:
   producer -> encoder -> renderer 每段都 memcpy
2) ZERO_COPY_PATH:
   上下游复用同一块缓冲指针，不做中间 memcpy

在 SoC 音视频里，零拷贝常见手段是:
- dmabuf fd 共享
- DRM PRIME / VAAPI surface 共享
- 硬件模块间直接引用同一物理缓冲
*/

/*
函数与参数速查:
1) run_copy_path(frames, frame_size):
   - frames: 模拟处理帧数。
   - frame_size: 单帧字节数。
   - 特征: 两段 memcpy（producer->encoder->renderer）。
2) run_zero_copy_path(frames, frame_size):
   - 同样帧数和帧大小，但中间不 memcpy，仅传递同一缓冲引用。
3) Stats:
   - bytes_copied: 累计拷贝字节数（量化 copy 压力）。
   - elapsed_us: 总耗时。
4) g_sink:
   - 防止编译器把示例循环优化掉，保证测试逻辑真实执行。
*/

// 函数: now_us
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

struct Stats {
    uint64_t bytes_copied; // 统计 memcpy 总字节数
    int64_t elapsed_us;    // 路径总耗时
};

static volatile uint8_t g_sink = 0;

// 函数: run_copy_path
// 参数: int frames, size_t frame_size
// 说明: 见本函数内部逻辑与文件顶部实验说明。
Stats run_copy_path(int frames, size_t frame_size) {
    Stats s;
    s.bytes_copied = 0;
    int64_t start = now_us();

    for (int i = 0; i < frames; ++i) {
        // 这里每一帧都申请三块缓冲，仅用于模拟“多段拷贝链路”。
        uint8_t* src = (uint8_t*)malloc(frame_size);
        uint8_t* enc_in = (uint8_t*)malloc(frame_size);
        uint8_t* render_in = (uint8_t*)malloc(frame_size);
        if (!src || !enc_in || !render_in) {
            fprintf(stderr, "malloc failed in copy path\n");
            exit(1);
        }

        memset(src, i & 0xFF, frame_size);
        memcpy(enc_in, src, frame_size);        // producer -> encoder copy
        memcpy(render_in, enc_in, frame_size);  // encoder -> renderer copy
        s.bytes_copied += frame_size * 2;

        g_sink ^= render_in[0];
        free(render_in);
        free(enc_in);
        free(src);
    }
    s.elapsed_us = now_us() - start;
    return s;
}

// 函数: run_zero_copy_path
// 参数: int frames, size_t frame_size
// 说明: 见本函数内部逻辑与文件顶部实验说明。
Stats run_zero_copy_path(int frames, size_t frame_size) {
    Stats s;
    s.bytes_copied = 0;
    int64_t start = now_us();

    for (int i = 0; i < frames; ++i) {
        // 这里只申请一块 shared，模拟“同一缓冲在多个模块间传递引用”。
        uint8_t* shared = (uint8_t*)malloc(frame_size);
        if (!shared) {
            fprintf(stderr, "malloc failed in zero-copy path\n");
            exit(1);
        }

        memset(shared, i & 0xFF, frame_size);
        // 同一指针被 producer -> encoder -> renderer 复用。
        // 这里没有 memcpy，因此 bytes_copied 不增加。
        g_sink ^= shared[0];
        free(shared);
    }
    s.elapsed_us = now_us() - start;
    return s;
}

// 函数: main
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main() {
    const int frames = 120;
    const size_t frame_size = 1280 * 720 * 3 / 2;

    Stats copy = run_copy_path(frames, frame_size);
    Stats zc = run_zero_copy_path(frames, frame_size);

    printf("COPY_PATH:      frames=%d copied=%.2f MB elapsed=%.2f ms\n", frames,
           copy.bytes_copied / (1024.0 * 1024.0), copy.elapsed_us / 1000.0);
    printf("ZERO_COPY_PATH: frames=%d copied=%.2f MB elapsed=%.2f ms\n", frames,
           zc.bytes_copied / (1024.0 * 1024.0), zc.elapsed_us / 1000.0);
    printf("sink=%u (avoid optimize-out)\n", (unsigned)g_sink);
    printf("解读: 零拷贝核心收益通常是降低 memcpy 成本、减少缓存污染、降低内存带宽压力。\n");
    return 0;
}
