#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include <sys/time.h>

/*
Week5 主题: DMA 双缓冲/多缓冲流水线模拟

模型:
CPU 填充缓冲(BUF_IN_CPU -> BUF_FILLED) -> DMA 消费缓冲(BUF_IN_DMA -> BUF_FREE)

你要从这个实验学到:
1) 双缓冲/多缓冲如何把“串行耗时”变成“流水线并行耗时”
2) 在抖动(jitter)存在时，缓冲数量与吞吐/延迟的关系
3) 怎么通过 cpu_wait/dma_wait 判断瓶颈方向
*/

/*
函数与参数速查:
1) run_serial_baseline_once(total_frames, fill_ms, dma_ms, jitter_pct):
   - 串行基线：每帧先 CPU 填充再 DMA 处理，无并行重叠。
2) run_buffered_pipeline_once(total_frames, fill_ms, dma_ms, jitter_pct, buffer_count):
   - 多缓冲流水线：CPU 与 DMA 并行工作，buffer_count 控制在途深度。
3) average_pipeline_stats(repeats, ...):
   - repeats: 重复次数，用平均值降低随机抖动对结果的影响。
4) sample_ms(base_ms, jitter_pct, seed):
   - base_ms: 基准耗时。
   - jitter_pct: 抖动幅度百分比（±区间采样）。
5) cpu_fill_thread/dma_thread(arg):
   - arg 实际是 DmaCtx*。
   - 两线程通过缓冲状态机协作，体现真实“生产-消费”关系。
6) 状态机含义:
   - BUF_FREE: 空闲可写
   - BUF_IN_CPU: CPU 正在填充
   - BUF_FILLED: 等 DMA 消费
   - BUF_IN_DMA: DMA 正在处理
*/

enum BufState {
    BUF_FREE = 0,   // 空闲，可被 CPU 填充
    BUF_IN_CPU = 1, // CPU 正在填充
    BUF_FILLED = 2, // 已填充，等待 DMA 消费
    BUF_IN_DMA = 3, // DMA 正在处理
};

struct DmaBuffer {
    int id;
    int frame_id;
    BufState state;
};

struct PipelineStats {
    double elapsed_ms;      // 总耗时
    double cpu_wait_ms;     // CPU 等待可用缓冲的总时长
    double dma_wait_ms;     // DMA 等待填充缓冲的总时长
    double cpu_work_ms;     // CPU 实际工作时长
    double dma_work_ms;     // DMA 实际工作时长
    int produced;           // 生产（填充）数量
    int consumed;           // 消费数量
    int max_filled_depth;   // FILLED 队列最大深度
};

struct DmaCtx {
    int total_frames;
    int fill_ms;
    int dma_ms;
    int jitter_pct; // 抖动百分比（例如 20 表示 +/-20%）

    int cpu_produced;
    int dma_consumed;
    int cpu_done;
    int max_filled_depth;

    int64_t cpu_wait_us;
    int64_t dma_wait_us;
    int64_t cpu_work_us;
    int64_t dma_work_us;

    std::vector<DmaBuffer> bufs;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
};

// 函数: now_us
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// 函数: ceil_div
// 参数: int a, int b
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int ceil_div(int a, int b) {
    return (a + b - 1) / b;
}

// 函数: sample_ms
// 参数: int base_ms, int jitter_pct, unsigned int* seed
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int sample_ms(int base_ms, int jitter_pct, unsigned int* seed) {
    if (jitter_pct <= 0 || base_ms <= 0) {
        return std::max(base_ms, 1);
    }
    // 在 [base-span, base+span] 区间内随机采样，模拟负载抖动。
    int span = ceil_div(base_ms * jitter_pct, 100);
    int min_ms = std::max(1, base_ms - span);
    int max_ms = std::max(min_ms, base_ms + span);
    int range = max_ms - min_ms + 1;
    return min_ms + (int)(rand_r(seed) % (unsigned int)range);
}

// 函数: find_state
// 参数: DmaCtx* ctx, BufState state
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int find_state(DmaCtx* ctx, BufState state) {
    for (size_t i = 0; i < ctx->bufs.size(); ++i) {
        if (ctx->bufs[i].state == state) {
            return (int)i;
        }
    }
    return -1;
}

// 函数: count_state
// 参数: const DmaCtx* ctx, BufState state
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int count_state(const DmaCtx* ctx, BufState state) {
    int count = 0;
    for (size_t i = 0; i < ctx->bufs.size(); ++i) {
        if (ctx->bufs[i].state == state) {
            ++count;
        }
    }
    return count;
}

// 函数: cpu_fill_thread
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* cpu_fill_thread(void* arg) {
    DmaCtx* ctx = (DmaCtx*)arg;
    unsigned int seed = (unsigned int)(now_us() ^ (uintptr_t)pthread_self() ^ 0x8F1BBCDCu);

    for (int frame = 0; frame < ctx->total_frames; ++frame) {
        pthread_mutex_lock(&ctx->mtx);
        int idx = -1;
        int64_t wait_begin = now_us();
        // 没有 FREE 缓冲时，CPU 只能等待 DMA 释放。
        while ((idx = find_state(ctx, BUF_FREE)) < 0) {
            pthread_cond_wait(&ctx->cond, &ctx->mtx);
        }
        ctx->cpu_wait_us += (now_us() - wait_begin);

        // 先占有该缓冲，再在锁外做耗时工作，避免长时间持锁。
        ctx->bufs[idx].state = BUF_IN_CPU;
        ctx->bufs[idx].frame_id = frame;
        pthread_mutex_unlock(&ctx->mtx);

        int fill_this_ms = sample_ms(ctx->fill_ms, ctx->jitter_pct, &seed);
        usleep(fill_this_ms * 1000);
        ctx->cpu_work_us += (int64_t)fill_this_ms * 1000;

        pthread_mutex_lock(&ctx->mtx);
        ctx->bufs[idx].state = BUF_FILLED; // 交给 DMA
        ctx->cpu_produced++;
        int filled_now = count_state(ctx, BUF_FILLED);
        if (filled_now > ctx->max_filled_depth) {
            ctx->max_filled_depth = filled_now;
        }
        pthread_cond_broadcast(&ctx->cond);
        pthread_mutex_unlock(&ctx->mtx);
    }

    pthread_mutex_lock(&ctx->mtx);
    ctx->cpu_done = 1;
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->mtx);
    return NULL;
}

// 函数: dma_thread
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* dma_thread(void* arg) {
    DmaCtx* ctx = (DmaCtx*)arg;
    unsigned int seed = (unsigned int)(now_us() ^ (uintptr_t)pthread_self() ^ 0xC4A4517Bu);

    while (1) {
        pthread_mutex_lock(&ctx->mtx);
        int idx = -1;
        int64_t wait_begin = now_us();
        // 没有可消费的 FILLED 缓冲时，DMA 等待 CPU 产出。
        while ((idx = find_state(ctx, BUF_FILLED)) < 0) {
            if (ctx->cpu_done) {
                pthread_mutex_unlock(&ctx->mtx);
                return NULL;
            }
            pthread_cond_wait(&ctx->cond, &ctx->mtx);
        }
        ctx->dma_wait_us += (now_us() - wait_begin);
        ctx->bufs[idx].state = BUF_IN_DMA;
        pthread_mutex_unlock(&ctx->mtx);

        int dma_this_ms = sample_ms(ctx->dma_ms, ctx->jitter_pct, &seed);
        usleep(dma_this_ms * 1000);
        ctx->dma_work_us += (int64_t)dma_this_ms * 1000;

        pthread_mutex_lock(&ctx->mtx);
        // DMA 消费后归还 FREE，形成循环。
        ctx->bufs[idx].state = BUF_FREE;
        ctx->bufs[idx].frame_id = -1;
        ctx->dma_consumed++;
        pthread_cond_broadcast(&ctx->cond);
        pthread_mutex_unlock(&ctx->mtx);
    }
}

// 函数: run_serial_baseline_once
// 参数: int total_frames, int fill_ms, int dma_ms, int jitter_pct
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static double run_serial_baseline_once(int total_frames, int fill_ms, int dma_ms, int jitter_pct) {
    unsigned int seed = (unsigned int)(now_us() ^ 0x3E4F93A5u);
    int64_t start = now_us();
    // 串行基线: 每帧严格先 fill 再 dma，不重叠。
    for (int i = 0; i < total_frames; ++i) {
        usleep(sample_ms(fill_ms, jitter_pct, &seed) * 1000);
        usleep(sample_ms(dma_ms, jitter_pct, &seed) * 1000);
    }
    return (now_us() - start) / 1000.0;
}

// 函数: run_buffered_pipeline_once
// 参数: int total_frames, int fill_ms, int dma_ms, int jitter_pct, int buffer_count
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static PipelineStats run_buffered_pipeline_once(int total_frames, int fill_ms, int dma_ms, int jitter_pct, int buffer_count) {
    DmaCtx ctx;
    ctx.total_frames = total_frames;
    ctx.fill_ms = fill_ms;
    ctx.dma_ms = dma_ms;
    ctx.jitter_pct = jitter_pct;
    ctx.cpu_produced = 0;
    ctx.dma_consumed = 0;
    ctx.cpu_done = 0;
    ctx.max_filled_depth = 0;
    ctx.cpu_wait_us = 0;
    ctx.dma_wait_us = 0;
    ctx.cpu_work_us = 0;
    ctx.dma_work_us = 0;
    // 至少 2 个缓冲才能形成“真正双缓冲”。
    ctx.bufs.resize((size_t)std::max(buffer_count, 2));
    for (size_t i = 0; i < ctx.bufs.size(); ++i) {
        ctx.bufs[i].id = (int)i;
        ctx.bufs[i].frame_id = -1;
        ctx.bufs[i].state = BUF_FREE;
    }
    pthread_mutex_init(&ctx.mtx, NULL);
    pthread_cond_init(&ctx.cond, NULL);

    pthread_t t_cpu;
    pthread_t t_dma;
    int64_t start = now_us();
    // 两线程并行即是流水线模型。
    pthread_create(&t_cpu, NULL, cpu_fill_thread, &ctx);
    pthread_create(&t_dma, NULL, dma_thread, &ctx);
    pthread_join(t_cpu, NULL);
    pthread_join(t_dma, NULL);
    int64_t elapsed_us = now_us() - start;

    PipelineStats stats;
    stats.elapsed_ms = elapsed_us / 1000.0;
    stats.cpu_wait_ms = ctx.cpu_wait_us / 1000.0;
    stats.dma_wait_ms = ctx.dma_wait_us / 1000.0;
    stats.cpu_work_ms = ctx.cpu_work_us / 1000.0;
    stats.dma_work_ms = ctx.dma_work_us / 1000.0;
    stats.produced = ctx.cpu_produced;
    stats.consumed = ctx.dma_consumed;
    stats.max_filled_depth = ctx.max_filled_depth;

    pthread_cond_destroy(&ctx.cond);
    pthread_mutex_destroy(&ctx.mtx);
    return stats;
}

// 函数: zero_stats
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static PipelineStats zero_stats() {
    PipelineStats s;
    s.elapsed_ms = 0.0;
    s.cpu_wait_ms = 0.0;
    s.dma_wait_ms = 0.0;
    s.cpu_work_ms = 0.0;
    s.dma_work_ms = 0.0;
    s.produced = 0;
    s.consumed = 0;
    s.max_filled_depth = 0;
    return s;
}

// 函数: average_pipeline_stats
// 参数: int repeats, int total_frames, int fill_ms, int dma_ms, int jitter_pct, int buffer_count
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static PipelineStats average_pipeline_stats(int repeats, int total_frames, int fill_ms, int dma_ms, int jitter_pct, int buffer_count) {
    PipelineStats sum = zero_stats();
    for (int i = 0; i < repeats; ++i) {
        PipelineStats one = run_buffered_pipeline_once(total_frames, fill_ms, dma_ms, jitter_pct, buffer_count);
        sum.elapsed_ms += one.elapsed_ms;
        sum.cpu_wait_ms += one.cpu_wait_ms;
        sum.dma_wait_ms += one.dma_wait_ms;
        sum.cpu_work_ms += one.cpu_work_ms;
        sum.dma_work_ms += one.dma_work_ms;
        sum.produced += one.produced;
        sum.consumed += one.consumed;
        sum.max_filled_depth = std::max(sum.max_filled_depth, one.max_filled_depth);
    }
    sum.elapsed_ms /= repeats;
    sum.cpu_wait_ms /= repeats;
    sum.dma_wait_ms /= repeats;
    sum.cpu_work_ms /= repeats;
    sum.dma_work_ms /= repeats;
    sum.produced /= repeats;
    sum.consumed /= repeats;
    return sum;
}

// 函数: arg_or_default
// 参数: int argc, char** argv, int index, int fallback
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int arg_or_default(int argc, char** argv, int index, int fallback) {
    if (index >= argc) {
        return fallback;
    }
    return atoi(argv[index]);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    int frames = arg_or_default(argc, argv, 1, 80);
    int fill_ms = arg_or_default(argc, argv, 2, 10);
    int dma_ms = arg_or_default(argc, argv, 3, 6);
    int max_buffers = arg_or_default(argc, argv, 4, 4);
    int jitter_pct = arg_or_default(argc, argv, 5, 0);
    int repeats = arg_or_default(argc, argv, 6, 1);

    if (frames <= 0) {
        frames = 1;
    }
    if (fill_ms <= 0) {
        fill_ms = 1;
    }
    if (dma_ms <= 0) {
        dma_ms = 1;
    }
    if (max_buffers < 2) {
        max_buffers = 2;
    }
    if (jitter_pct < 0) {
        jitter_pct = 0;
    }
    if (repeats <= 0) {
        repeats = 1;
    }

    // 先算串行基线，再对比多缓冲流水线加速比。
    double serial_ms = 0.0;
    for (int i = 0; i < repeats; ++i) {
        serial_ms += run_serial_baseline_once(frames, fill_ms, dma_ms, jitter_pct);
    }
    serial_ms /= repeats;

    // 两阶段流水线理想值（忽略抖动/调度开销）:
    // 首帧耗时 fill+dma，后续每帧按慢阶段节拍推进。
    double ideal_pipeline_ms = (fill_ms + dma_ms) + (frames - 1) * std::max(fill_ms, dma_ms);

    printf("config: frames=%d fill=%dms dma=%dms jitter=%d%% repeats=%d\n",
           frames, fill_ms, dma_ms, jitter_pct, repeats);
    printf("serial(avg): %.2f ms\n", serial_ms);
    printf("ideal-2stage(no-jitter): %.2f ms\n", ideal_pipeline_ms);
    printf("\n");
    printf("buffers | time(ms) | speedup | throughput(f/s) | cpu_wait(ms) | dma_wait(ms) | cpu_util | dma_util | max_filled\n");
    printf("--------+----------+---------+-----------------+--------------+--------------+----------+----------+-----------\n");

    // 扫描缓冲数量，观察吞吐与等待时间变化。
    for (int b = 2; b <= max_buffers; ++b) {
        PipelineStats s = average_pipeline_stats(repeats, frames, fill_ms, dma_ms, jitter_pct, b);
        double speedup = serial_ms / s.elapsed_ms;
        double throughput = (frames * 1000.0) / s.elapsed_ms;
        double cpu_util = s.cpu_work_ms / s.elapsed_ms * 100.0;
        double dma_util = s.dma_work_ms / s.elapsed_ms * 100.0;
        printf("%7d | %8.2f | %7.2fx | %15.2f | %12.2f | %12.2f | %7.2f%% | %7.2f%% | %9d\n",
               b, s.elapsed_ms, speedup, throughput, s.cpu_wait_ms, s.dma_wait_ms, cpu_util, dma_util, s.max_filled_depth);
    }

    printf("\nusage: ./week_05 [frames] [fill_ms] [dma_ms] [max_buffers] [jitter_pct] [repeats]\n");
    printf("解读: buffer 增大不一定继续提速，可能只是在增加排队与内存占用。\n");
    return 0;
}
