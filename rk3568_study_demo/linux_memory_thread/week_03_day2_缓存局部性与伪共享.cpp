#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

/*
Week3 Day2: 缓存局部性 与 伪共享（False Sharing）

为什么这个主题对 SoC 音视频非常关键：
1) 音视频链路是高带宽流水线，CPU 前后处理（色彩转换、OSD、拷贝）常被内存访问模式限制。
2) 多线程统计/调度结构若布局不当，会因伪共享导致吞吐下降和抖动上升。
3) 你做性能优化时，经常会遇到“CPU 占用不高，但速度上不去”，背后可能就是缓存行为。

这个文件包含两个子实验：
1) false_sharing:
   - 对比 “计数器挤在同一缓存行” vs “按缓存行分离”
   - 验证伪共享对吞吐的影响
2) locality:
   - 对比顺序访问 vs 随机访问
   - 观察缓存局部性对访问效率影响

编译:
  g++ -std=c++11 -O2 -pthread week_03_day2_缓存局部性与伪共享.cpp -o week_03_day2_cache_lab

运行:
  ./week_03_day2_cache_lab false_sharing 4 30000000
  ./week_03_day2_cache_lab locality 64 3
*/

/*
函数与参数速查:
1) run_false_sharing(threads, loops):
   - threads: 参与更新计数器的线程数。
   - loops: 每线程自增次数。
   - 对比 PACKED vs PADDED 布局下吞吐差异。
2) fs_worker(arg):
   - arg 实际为 FsArg*，包含 tid 和共享上下文指针。
   - tid 决定当前线程写哪个计数槽位。
3) run_locality(size_mb, rounds):
   - size_mb: 测试数组大小。
   - rounds: 重复访问轮数。
   - 对比顺序访问与随机访问带宽。
4) arg_or_default(argc, argv, idx, dft):
   - 统一参数解析工具，idx 越界时返回 dft。
*/

// 函数: mono_ns
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t mono_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// 函数: arg_or_default
// 参数: int argc, char** argv, int idx, int dft
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) return dft;
    return atoi(argv[idx]);
}

// ------------------------------------------------------
// Demo A: false sharing
// ------------------------------------------------------
//
// 背景:
// 多个线程各自更新“不同变量”，但如果这些变量落在同一缓存行(通常64B)，
// 仍会触发缓存一致性竞争（cache line bouncing）。
// 
// 这就叫伪共享：逻辑上没共享同一变量，物理上共享同一缓存行。
//
// 工程对应:
// - 多线程统计字段（fps/drop/queue_depth）若结构体挤在一起，常见伪共享。
// - 解决手段：padding / alignas(64) / 每线程独立分区。

struct PackedCounter {
    volatile uint64_t v; // 紧凑布局，多个计数器容易同缓存行
};

// 64B 对齐+填充，尽量让每个计数器占独立缓存行
struct PaddedCounter {
    alignas(64) volatile uint64_t v;
    char pad[64 - sizeof(uint64_t)];
};

struct FsCtx {
    int threads;
    int64_t loops;
    int padded; // 0: packed, 1: padded
    PackedCounter* packed;
    PaddedCounter* padded_arr;
    pthread_barrier_t barrier;
};

struct FsArg {
    FsCtx* ctx;
    int tid;
};

// 函数: fs_worker
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* fs_worker(void* arg) {
    FsArg* a = (FsArg*)arg;
    FsCtx* c = a->ctx;
    int tid = a->tid;
    pthread_barrier_wait(&c->barrier);

    if (!c->padded) {
        for (int64_t i = 0; i < c->loops; ++i) {
            c->packed[tid].v++;
        }
    } else {
        for (int64_t i = 0; i < c->loops; ++i) {
            c->padded_arr[tid].v++;
        }
    }
    return NULL;
}

// 函数: run_false_sharing
// 参数: int threads, int64_t loops
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void run_false_sharing(int threads, int64_t loops) {
    if (threads <= 0) threads = 1;
    if (threads > 64) threads = 64;
    if (loops <= 0) loops = 1;

    PackedCounter* packed = (PackedCounter*)calloc((size_t)threads, sizeof(PackedCounter));
    PaddedCounter* padded = (PaddedCounter*)calloc((size_t)threads, sizeof(PaddedCounter));
    if (!packed || !padded) {
        fprintf(stderr, "alloc failed\n");
        exit(1);
    }

    for (int mode = 0; mode < 2; ++mode) {
        FsCtx c;
        memset(&c, 0, sizeof(c));
        c.threads = threads;
        c.loops = loops;
        c.padded = mode;
        c.packed = packed;
        c.padded_arr = padded;
        pthread_barrier_init(&c.barrier, NULL, (unsigned int)threads);

        pthread_t tids[64];
        FsArg args[64];
        int64_t t0 = mono_ns();
        for (int i = 0; i < threads; ++i) {
            args[i].ctx = &c;
            args[i].tid = i;
            pthread_create(&tids[i], NULL, fs_worker, &args[i]);
        }
        for (int i = 0; i < threads; ++i) {
            pthread_join(tids[i], NULL);
        }
        int64_t t1 = mono_ns();

        uint64_t sum = 0;
        if (!mode) {
            for (int i = 0; i < threads; ++i) sum += packed[i].v;
        } else {
            for (int i = 0; i < threads; ++i) sum += padded[i].v;
        }
        double elapsed_ms = (t1 - t0) / 1000000.0;
        double mops = (threads * loops / 1000000.0) / (elapsed_ms / 1000.0);
        printf("[false_sharing][%s] threads=%d loops=%lld elapsed=%.2fms throughput=%.2f Mops/s sum=%llu\n",
               mode ? "PADDED" : "PACKED", threads, (long long)loops, elapsed_ms, mops,
               (unsigned long long)sum);

        pthread_barrier_destroy(&c.barrier);
        memset(packed, 0, (size_t)threads * sizeof(PackedCounter));
        memset(padded, 0, (size_t)threads * sizeof(PaddedCounter));
    }

    printf("解读: 若 PADDED 明显快于 PACKED，说明伪共享在你的平台上影响显著。\n");
    free(packed);
    free(padded);
}

// ------------------------------------------------------
// Demo B: locality
// ------------------------------------------------------
//
// 背景:
// 顺序访问通常更容易命中缓存和硬件预取；随机访问会打散局部性。
// 在视频处理里，像素按行连续访问通常更友好，乱跳访问会变慢。

// 函数: run_locality
// 参数: int size_mb, int rounds
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void run_locality(int size_mb, int rounds) {
    if (size_mb <= 0) size_mb = 64;
    if (rounds <= 0) rounds = 1;

    size_t bytes = (size_t)size_mb * 1024 * 1024;
    size_t n = bytes / sizeof(uint64_t);
    if (n < 1024) n = 1024;
    std::vector<uint64_t> arr(n, 1);
    std::vector<size_t> idx(n);
    for (size_t i = 0; i < n; ++i) idx[i] = i;

    // Fisher-Yates 随机置换索引
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = (size_t)(rand() % (int)(i + 1));
        std::swap(idx[i], idx[j]);
    }

    volatile uint64_t sink = 0;
    int64_t t0 = mono_ns();
    for (int r = 0; r < rounds; ++r) {
        for (size_t i = 0; i < n; ++i) {
            sink += arr[i];
        }
    }
    int64_t t1 = mono_ns();
    for (int r = 0; r < rounds; ++r) {
        for (size_t i = 0; i < n; ++i) {
            sink += arr[idx[i]];
        }
    }
    int64_t t2 = mono_ns();

    double seq_ms = (t1 - t0) / 1000000.0;
    double rnd_ms = (t2 - t1) / 1000000.0;
    double total_mb = (double)size_mb * rounds;
    printf("[locality] size=%dMB rounds=%d\n", size_mb, rounds);
    printf("sequential: %.2fms, bandwidth=%.2f MB/s\n", seq_ms, total_mb / (seq_ms / 1000.0));
    printf("random    : %.2fms, bandwidth=%.2f MB/s\n", rnd_ms, total_mb / (rnd_ms / 1000.0));
    printf("sink=%llu\n", (unsigned long long)sink);
    printf("解读: sequential 通常更快，体现缓存局部性与预取优势。\n");
}

// 函数: usage
// 参数: const char* bin
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void usage(const char* bin) {
    printf("usage:\n");
    printf("  %s false_sharing [threads] [loops_per_thread]\n", bin);
    printf("  %s locality [size_mb] [rounds]\n", bin);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    srand((unsigned int)time(NULL));

    if (strcmp(argv[1], "false_sharing") == 0) {
        int threads = arg_or_default(argc, argv, 2, 4);
        int loops = arg_or_default(argc, argv, 3, 30000000);
        run_false_sharing(threads, loops);
        return 0;
    }
    if (strcmp(argv[1], "locality") == 0) {
        int size_mb = arg_or_default(argc, argv, 2, 64);
        int rounds = arg_or_default(argc, argv, 3, 3);
        run_locality(size_mb, rounds);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
