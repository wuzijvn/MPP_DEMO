#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <vector>

/*
Week6 Day0: Cache Demo 带练版

目标：
1) 让你亲手改少量代码，完成一次可验证实验。
2) 练会“现象 -> 假设 -> A/B -> 指标 -> 结论”。

编译：
  g++ -std=c++11 -O2 week_06_day0_cache_demo_带练版.cpp -o week_06_day0_cache_lab

运行：
  ./week_06_day0_cache_lab 64 3 64

参数：
  argv[1] size_mb      默认 64
  argv[2] rounds       默认 3
  argv[3] stride_bytes 默认 64

练习任务（你要自己改）：
1) 把 build_stride_idx() 改成“更不友好缓存”的版本（例如大步长+跨页跳转）。
2) 对比 sequential / random / stride 三种结果。
3) 给出一句量化结论（不要写“感觉”）。
*/

static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) return dft;
    return atoi(argv[idx]);
}

static int64_t mono_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static std::vector<size_t> build_random_idx(size_t n) {
    std::vector<size_t> idx(n);
    for (size_t i = 0; i < n; ++i) idx[i] = i;
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = (size_t)(rand() % (int)(i + 1));
        std::swap(idx[i], idx[j]);
    }
    return idx;
}

/*
练习入口：
你先运行默认版本，再改这里，做 A/B 对比。
默认实现是“stride 扫描”，缓存友好性介于 sequential 与 random 之间。
*/
static std::vector<size_t> build_stride_idx(size_t n, size_t stride_elems) {
    if (stride_elems == 0) stride_elems = 1;
    std::vector<size_t> idx;
    idx.reserve(n);
    for (size_t start = 0; start < stride_elems; ++start) {
        for (size_t i = start; i < n; i += stride_elems) {
            idx.push_back(i);
        }
    }
    return idx;
}

static double run_idx_read(const std::vector<uint64_t>& arr,
                           const std::vector<size_t>& idx,
                           int rounds,
                           volatile uint64_t* sink) {
    int64_t t0 = mono_ns();
    for (int r = 0; r < rounds; ++r) {
        for (size_t i = 0; i < idx.size(); ++i) {
            *sink += arr[idx[i]];
        }
    }
    int64_t t1 = mono_ns();
    return (t1 - t0) / 1000000.0;
}

static double run_seq_read(const std::vector<uint64_t>& arr,
                           int rounds,
                           volatile uint64_t* sink) {
    int64_t t0 = mono_ns();
    for (int r = 0; r < rounds; ++r) {
        for (size_t i = 0; i < arr.size(); ++i) {
            *sink += arr[i];
        }
    }
    int64_t t1 = mono_ns();
    return (t1 - t0) / 1000000.0;
}

static void print_bandwidth(const char* tag, double elapsed_ms, int size_mb, int rounds) {
    double total_mb = (double)size_mb * rounds;
    double bw = total_mb / (elapsed_ms / 1000.0);
    printf("%-10s elapsed=%.2fms bandwidth=%.2f MB/s\n", tag, elapsed_ms, bw);
}

int main(int argc, char** argv) {
    int size_mb = arg_or_default(argc, argv, 1, 64);
    int rounds = arg_or_default(argc, argv, 2, 3);
    int stride_bytes = arg_or_default(argc, argv, 3, 64);
    if (size_mb <= 0) size_mb = 64;
    if (rounds <= 0) rounds = 1;
    if (stride_bytes <= 0) stride_bytes = 64;

    size_t bytes = (size_t)size_mb * 1024 * 1024;
    size_t n = bytes / sizeof(uint64_t);
    if (n < 1024) n = 1024;

    std::vector<uint64_t> arr(n, 1);
    std::vector<size_t> rnd = build_random_idx(n);
    size_t stride_elems = (size_t)stride_bytes / sizeof(uint64_t);
    if (stride_elems == 0) stride_elems = 1;
    std::vector<size_t> strd = build_stride_idx(n, stride_elems);

    volatile uint64_t sink = 0;
    double seq_ms = run_seq_read(arr, rounds, &sink);
    double rnd_ms = run_idx_read(arr, rnd, rounds, &sink);
    double str_ms = run_idx_read(arr, strd, rounds, &sink);

    printf("[config] size=%dMB rounds=%d stride_bytes=%d\n", size_mb, rounds, stride_bytes);
    print_bandwidth("sequential", seq_ms, size_mb, rounds);
    print_bandwidth("random", rnd_ms, size_mb, rounds);
    print_bandwidth("stride", str_ms, size_mb, rounds);
    printf("sink=%llu\n", (unsigned long long)sink);

    printf("\n[结论模板]\n");
    printf("1) 在本机上，_____ 比 _____ 快 ____%%。\n");
    printf("2) 说明当前瓶颈更偏向 _____（缓存局部性 / 随机访问 / 其他）。\n");
    printf("3) 对实际音视频链路的动作：优先优化 _____。\n");
    return 0;
}

