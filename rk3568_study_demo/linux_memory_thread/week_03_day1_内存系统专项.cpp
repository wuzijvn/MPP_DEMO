#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <deque>
#include <vector>

/*
Week3 Day1: 内存系统专项（面向 SoC 音视频）

这个文件把“内存基础”拆成三个你工作里会频繁遇到的实验：

1) page_fault
   验证“第一次触页慢、缺页多；再次访问快、缺页少”
   对应实际: 大块视频缓冲首次访问卡顿、预热策略评估

2) alloc_pattern
   对比“每帧 malloc/free” vs “固定池复用”
   对应实际: 帧缓冲管理、碎片风险、实时性抖动

3) alignment
   展示地址对齐与带宽测试基础
   对应实际: DMA/硬件模块常要求对齐，缓存友好访问很重要

编译:
  g++ -std=c++11 -O2 -pthread week_03_day1_内存系统专项.cpp -o week_03_day1_mem_lab

运行示例:
  ./week_03_day1_mem_lab page_fault 256 2
  ./week_03_day1_mem_lab alloc_pattern 240 3072 8
  ./week_03_day1_mem_lab alignment 4194304 200
*/

/*
函数与参数速查:
1) run_page_fault(argc, argv):
   - argv[2]=size_mb: 申请内存大小（MB）。
   - argv[3]=second_passes: 第二轮复访次数。
   - 用 first_touch vs second_touch 对比缺页与耗时。
2) touch_pages(p, bytes, passes):
   - p: 目标内存地址；bytes: 总字节；passes: 遍历轮数。
   - 返回 FaultStat（elapsed/minflt/majflt 变化量）。
3) run_alloc_pattern(argc, argv):
   - argv[2]=frames; argv[3]=frame_kb; argv[4]=pool_count。
   - 对比每帧 malloc/free 和固定池复用模式。
4) run_malloc_free_pattern(frames, frame_bytes):
   - 关键观察 alloc_calls 是否随帧数线性增长。
5) run_pool_pattern(frames, frame_bytes, pool_count):
   - 关键观察 alloc_calls 是否稳定为 pool_count。
6) run_alignment(argc, argv):
   - argv[2]=bytes; argv[3]=loops。
   - 对比 64B 对齐分配与普通 malloc 地址及写带宽。
*/

// 函数: now_us
// 作用: 返回当前时间(微秒)用于性能计时。
// 参数: 无。
static int64_t now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// 函数: arg_or_default
// 作用: 从命令行读取整数参数；越界时返回默认值。
// 参数:
// - argc/argv: main 传入参数。
// - idx: 目标参数下标。
// - dft: 参数缺失时默认值。
static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) return dft;
    return atoi(argv[idx]);
}

// -----------------------------
// Demo A: page_fault
// -----------------------------

struct FaultStat {
    int64_t elapsed_us;
    long minflt_delta;
    long majflt_delta;
};

// 函数: touch_pages
// 作用: 按页触碰内存并统计缺页变化与耗时。
// 参数:
// - p: 起始内存地址。
// - bytes: 总字节数。
// - passes: 遍历轮数。
static FaultStat touch_pages(uint8_t* p, size_t bytes, int passes) {
    const size_t kPage = 4096;
    rusage ru0, ru1;
    getrusage(RUSAGE_SELF, &ru0);
    int64_t t0 = now_us();

    for (int pass = 0; pass < passes; ++pass) {
        for (size_t off = 0; off < bytes; off += kPage) {
            // 每页写 1 字节，强制触页
            p[off] = (uint8_t)((pass + off) & 0xFF);
        }
    }

    int64_t t1 = now_us();
    getrusage(RUSAGE_SELF, &ru1);

    FaultStat s;
    s.elapsed_us = t1 - t0;
    s.minflt_delta = ru1.ru_minflt - ru0.ru_minflt;
    s.majflt_delta = ru1.ru_majflt - ru0.ru_majflt;
    return s;
}

// 函数: run_page_fault
// 作用: 执行“冷页首次触碰 vs 热页复访”实验。
// 参数:
// - argc/argv: 命令行参数；argv[2]=size_mb, argv[3]=second_passes。
static int run_page_fault(int argc, char** argv) {
    int mb = arg_or_default(argc, argv, 2, 256);
    int passes = arg_or_default(argc, argv, 3, 2);
    if (mb <= 0) mb = 64;
    if (passes <= 0) passes = 1;

    size_t bytes = (size_t)mb * 1024 * 1024;
    uint8_t* mem = (uint8_t*)mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // 第一轮: 冷页触发
    FaultStat first = touch_pages(mem, bytes, 1);
    // 第二轮: 热页复访
    FaultStat second = touch_pages(mem, bytes, passes);

    printf("[page_fault] size=%dMB\n", mb);
    printf("first_touch : elapsed=%.2fms minflt=%ld majflt=%ld\n",
           first.elapsed_us / 1000.0, first.minflt_delta, first.majflt_delta);
    printf("second_touch: elapsed=%.2fms minflt=%ld majflt=%ld (passes=%d)\n",
           second.elapsed_us / 1000.0, second.minflt_delta, second.majflt_delta, passes);
    printf("解读: first_touch 慢且 minflt 高是正常现象，说明页按需分配被触发。\n");

    munmap(mem, bytes);
    return 0;
}

// -----------------------------
// Demo B: alloc_pattern
// -----------------------------

struct AllocStat {
    double elapsed_ms;
    uint64_t bytes_touched;
    int alloc_calls;
};

// 函数: touch_frame_like_workload
// 作用: 模拟“写整帧”工作负载（按页触碰）。
// 参数:
// - p: 帧缓冲地址。
// - frame_bytes: 帧大小(字节)。
// - seed: 写入数据扰动种子，防止编译器优化。
static uint64_t touch_frame_like_workload(uint8_t* p, size_t frame_bytes, int seed) {
    // 用“每页触碰”模拟写帧:
    // 真实编码前处理通常会遍历整帧数据，而不是只写几个字节。
    const size_t kPage = 4096;
    uint64_t touched = 0;
    for (size_t off = 0; off < frame_bytes; off += kPage) {
        p[off] = (uint8_t)((seed + off) & 0xFF);
        touched++;
    }
    // 兜底保证尾部也触碰到
    p[frame_bytes - 1] = (uint8_t)(seed & 0xFF);
    touched++;
    return touched;
}

// 函数: run_malloc_free_pattern
// 作用: 每帧都 malloc/free，测量动态分配路径成本。
// 参数:
// - frames: 总帧数。
// - frame_bytes: 单帧字节数。
static AllocStat run_malloc_free_pattern(int frames, size_t frame_bytes) {
    int64_t t0 = now_us();
    uint64_t touched = 0;
    int alloc_calls = 0;
    for (int i = 0; i < frames; ++i) {
        uint8_t* p = (uint8_t*)malloc(frame_bytes);
        if (!p) {
            perror("malloc");
            exit(1);
        }
        alloc_calls++;
        touched += touch_frame_like_workload(p, frame_bytes, i);
        free(p);
    }
    int64_t t1 = now_us();
    AllocStat s;
    s.elapsed_ms = (t1 - t0) / 1000.0;
    s.bytes_touched = touched;
    s.alloc_calls = alloc_calls;
    return s;
}

// 函数: run_pool_pattern
// 作用: 预分配固定缓冲池并循环复用，测量池化路径成本。
// 参数:
// - frames: 总帧数。
// - frame_bytes: 单帧字节数。
// - pool_count: 池中缓冲数量。
static AllocStat run_pool_pattern(int frames, size_t frame_bytes, int pool_count) {
    std::vector<uint8_t*> pool;
    pool.resize((size_t)pool_count);
    int alloc_calls = 0;
    for (int i = 0; i < pool_count; ++i) {
        pool[(size_t)i] = (uint8_t*)malloc(frame_bytes);
        if (!pool[(size_t)i]) {
            perror("malloc pool");
            exit(1);
        }
        alloc_calls++;
    }

    int64_t t0 = now_us();
    uint64_t touched = 0;
    for (int i = 0; i < frames; ++i) {
        // 循环复用 pool 中的缓冲
        uint8_t* p = pool[(size_t)(i % pool_count)];
        touched += touch_frame_like_workload(p, frame_bytes, i);
    }
    int64_t t1 = now_us();

    for (int i = 0; i < pool_count; ++i) {
        free(pool[(size_t)i]);
    }
    AllocStat s;
    s.elapsed_ms = (t1 - t0) / 1000.0;
    s.bytes_touched = touched;
    s.alloc_calls = alloc_calls;
    return s;
}

// 函数: run_alloc_pattern
// 作用: 对比 malloc/free 与 pool reuse 两种分配模式。
// 参数:
// - argc/argv: argv[2]=frames, argv[3]=frame_kb, argv[4]=pool_count。
static int run_alloc_pattern(int argc, char** argv) {
    int frames = arg_or_default(argc, argv, 2, 240);
    int frame_kb = arg_or_default(argc, argv, 3, 3072); // 约等于 1080p YUV420 一帧
    int pool_count = arg_or_default(argc, argv, 4, 8);
    if (frames <= 0) frames = 1;
    if (frame_kb <= 0) frame_kb = 64;
    if (pool_count <= 0) pool_count = 2;

    size_t frame_bytes = (size_t)frame_kb * 1024;
    AllocStat a = run_malloc_free_pattern(frames, frame_bytes);
    AllocStat b = run_pool_pattern(frames, frame_bytes, pool_count);

    printf("[alloc_pattern] frames=%d frame=%dKB pool_count=%d\n", frames, frame_kb, pool_count);
    printf("malloc/free : elapsed=%.2fms alloc_calls=%d\n", a.elapsed_ms, a.alloc_calls);
    printf("pool reuse  : elapsed=%.2fms alloc_calls=%d\n", b.elapsed_ms, b.alloc_calls);
    printf("解读: pool 的关键价值是显著减少 alloc_calls，降低分配抖动与碎片风险；耗时受平台实现影响。\n");
    return 0;
}

// -----------------------------
// Demo C: alignment
// -----------------------------

// 函数: run_alignment
// 作用: 比较对齐分配与普通分配的地址对齐情况和写带宽。
// 参数:
// - argc/argv: argv[2]=bytes, argv[3]=loops。
static int run_alignment(int argc, char** argv) {
    int bytes = arg_or_default(argc, argv, 2, 4 * 1024 * 1024);
    int loops = arg_or_default(argc, argv, 3, 200);
    if (bytes <= 0) bytes = 1024 * 1024;
    if (loops <= 0) loops = 1;

    // 对齐分配（64 字节对齐）大颗粒度对齐
    void* a_ptr = NULL;
    if (posix_memalign(&a_ptr, 64, (size_t)bytes) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        return 1;
    }
    // 普通分配（不保证 64 字节对齐）
    uint8_t* u_ptr = (uint8_t*)malloc((size_t)bytes);
    if (!u_ptr) {
        free(a_ptr);
        perror("malloc");
        return 1;
    }

    uint8_t* aligned = (uint8_t*)a_ptr;
    memset(aligned, 0x11, (size_t)bytes);
    memset(u_ptr, 0x22, (size_t)bytes);

    // 简单带宽写测试
    int64_t t0 = now_us();
    for (int i = 0; i < loops; ++i) {
        memset(aligned, i & 0xFF, (size_t)bytes);
    }
    int64_t t1 = now_us();
    for (int i = 0; i < loops; ++i) {
        memset(u_ptr, i & 0xFF, (size_t)bytes);
    }
    int64_t t2 = now_us();

    double mb = (bytes * loops) / (1024.0 * 1024.0);
    double aligned_ms = (t1 - t0) / 1000.0;
    double unaligned_ms = (t2 - t1) / 1000.0;
    printf("[alignment] bytes=%d loops=%d\n", bytes, loops);
    printf("aligned addr=%p (mod64=%ld)\n", (void*)aligned, (long)((uintptr_t)aligned % 64));
    printf("malloc  addr=%p (mod64=%ld)\n", (void*)u_ptr, (long)((uintptr_t)u_ptr % 64));
    printf("aligned memset  : %.2fms, bandwidth=%.2f MB/s\n", aligned_ms, mb / (aligned_ms / 1000.0));
    printf("malloc  memset  : %.2fms, bandwidth=%.2f MB/s\n", unaligned_ms, mb / (unaligned_ms / 1000.0));
    printf("解读: 对齐对 DMA/SIMD 更关键；带宽差异会受平台与实现影响。\n");

    free(u_ptr);
    free(a_ptr);
    return 0;
}

// 函数: usage
// 作用: 打印命令行使用帮助。
// 参数:
// - bin: 程序名。
static void usage(const char* bin) {
    printf("usage:\n");
    printf("  %s page_fault [size_mb] [second_passes]\n", bin);
    printf("  %s alloc_pattern [frames] [frame_kb] [pool_count]\n", bin);
    printf("  %s alignment [bytes] [loops]\n", bin);
}

// 函数: main
// 作用: 解析子命令并分发到 page_fault / alloc_pattern / alignment。
// 参数:
// - argc/argv: 命令行参数。
int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "page_fault") == 0) {
        return run_page_fault(argc, argv);
    }
    if (strcmp(argv[1], "alloc_pattern") == 0) {
        return run_alloc_pattern(argc, argv);
    }
    if (strcmp(argv[1], "alignment") == 0) {
        return run_alignment(argc, argv);
    }

    usage(argv[0]);
    return 1;
}
