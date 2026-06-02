#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atomic>
#include <string.h>
#include <vector>

/*
Week2 Day5: 原子操作与内存序（工程入门版）

你需要掌握的关键结论:
1) std::atomic 解决“单变量并发读写”的原子性问题
2) mutex 解决“多变量/复杂临界区一致性”问题
3) memory_order_relaxed 常用于统计计数
4) 跨线程发布数据常用 release/acquire

本文件包含两个实验:
Experiment A: Counter Benchmark
  - MUTEX
  - ATOMIC_RELAXED
  - ATOMIC_SEQ_CST
  比较吞吐与正确性

Experiment B: Publish Pattern
  - writer: payload = X; ready.store(1, release)
  - reader: while(!ready.load(acquire)); read payload
  演示 release/acquire 的常见发布-订阅用法

阅读顺序:
1) 先看 counter_worker（对比 mutex 与 atomic）
2) 再看 publish_writer / publish_reader（release-acquire）
3) 最后看 main 的结果表与解读
*/

/*
函数与参数速查:
1) run_counter_bench(mode, threads, loops):
   - mode: MUTEX / ATOMIC_RELAXED / ATOMIC_SEQ_CST。
   - threads: 并发线程数。
   - loops: 每线程增量次数。
   - 输出: 正确性(expected/actual) + 吞吐(mops)。
2) counter_worker(arg):
   - arg 实际是 CounterBenchCtx*。
   - 同样工作负载下切换不同同步方式，确保对比公平。
3) run_publish_experiment(rounds):
   - rounds: 发布-订阅实验重复轮数。
   - 观察 release/acquire 模式下，读线程是否能稳定看到完整 payload。
4) publish_writer/publish_reader(arg):
   - arg 是 PublishCtx*；writer 先写 payload 再 release ready，
     reader acquire ready 后再读 payload。
*/

enum BenchMode {
    BENCH_MUTEX = 0,
    BENCH_ATOMIC_RELAXED = 1,
    BENCH_ATOMIC_SEQ_CST = 2,
};

struct CounterBenchCtx {
    BenchMode mode; // 基准模式
    int thread_num; // 线程数
    int64_t loops;  // 每线程循环次数

    int64_t plain_counter;               // mutex 模式计数器
    std::atomic<int64_t> atomic_counter; // atomic 模式计数器
    pthread_mutex_t mtx;                 // mutex 模式锁
    pthread_barrier_t barrier;           // 同步起跑
};

struct CounterBenchResult {
    BenchMode mode;
    int threads;
    int64_t loops;
    int64_t expected;
    int64_t actual;
    double elapsed_ms;
    double mops;
};

// 函数: now_us
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t now_us() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// 函数: bench_mode_name
// 参数: BenchMode m
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static const char* bench_mode_name(BenchMode m) {
    switch (m) {
        case BENCH_MUTEX:
            return "MUTEX";
        case BENCH_ATOMIC_RELAXED:
            return "ATOMIC_RELAXED";
        case BENCH_ATOMIC_SEQ_CST:
            return "ATOMIC_SEQ_CST";
        default:
            return "UNKNOWN";
    }
}

// 函数: counter_worker
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* counter_worker(void* arg) {
    CounterBenchCtx* c = (CounterBenchCtx*)arg;
    pthread_barrier_wait(&c->barrier);

    for (int64_t i = 0; i < c->loops; ++i) {
        if (c->mode == BENCH_MUTEX) {
            pthread_mutex_lock(&c->mtx);
            c->plain_counter++;
            pthread_mutex_unlock(&c->mtx);
        } else if (c->mode == BENCH_ATOMIC_RELAXED) {
            c->atomic_counter.fetch_add(1, std::memory_order_relaxed);
        } else {
            c->atomic_counter.fetch_add(1, std::memory_order_seq_cst);
        }
    }
    return NULL;
}

// 函数: run_counter_bench
// 参数: BenchMode mode, int threads, int64_t loops
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static CounterBenchResult run_counter_bench(BenchMode mode, int threads, int64_t loops) {
    CounterBenchCtx c;
    memset(&c, 0, sizeof(c));
    c.mode = mode;
    c.thread_num = threads;
    c.loops = loops;
    c.plain_counter = 0;
    c.atomic_counter.store(0, std::memory_order_relaxed);
    pthread_mutex_init(&c.mtx, NULL);
    pthread_barrier_init(&c.barrier, NULL, (unsigned int)threads);

    pthread_t tids[128];
    if (threads > 128) {
        threads = 128;
        c.thread_num = 128;
    }
    int64_t begin = now_us();
    for (int i = 0; i < threads; ++i) {
        pthread_create(&tids[i], NULL, counter_worker, &c);
    }
    for (int i = 0; i < threads; ++i) {
        pthread_join(tids[i], NULL);
    }
    int64_t end = now_us();

    CounterBenchResult r;
    r.mode = mode;
    r.threads = threads;
    r.loops = loops;
    r.expected = (int64_t)threads * loops;
    r.actual = (mode == BENCH_MUTEX)
                   ? c.plain_counter
                   : c.atomic_counter.load(std::memory_order_relaxed);
    r.elapsed_ms = (end - begin) / 1000.0;
    r.mops = (r.expected / 1000000.0) / (r.elapsed_ms / 1000.0);

    pthread_barrier_destroy(&c.barrier);
    pthread_mutex_destroy(&c.mtx);
    return r;
}

struct PublishCtx {
    int rounds;            // 往返轮数
    std::atomic<int> ready;// 发布标记（0/1）
    int payload;           // 被发布的数据
    volatile int errors;   // 错误计数（仅实验用途）
};

// 函数: publish_writer
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* publish_writer(void* arg) {
    PublishCtx* c = (PublishCtx*)arg;
    for (int i = 1; i <= c->rounds; ++i) {
        // 1) 写真实数据
        c->payload = i;
        // 2) 发布 ready=1（release）
        // 语义: 在这之前的写(payload) 对 acquire 读 ready 的线程可见。
        c->ready.store(1, std::memory_order_release);
        // 等 reader 消费后把 ready 清回 0
        while (c->ready.load(std::memory_order_relaxed) != 0) {
            sched_yield();
        }
    }
    return NULL;
}

// 函数: publish_reader
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* publish_reader(void* arg) {
    PublishCtx* c = (PublishCtx*)arg;
    int last = 0;
    for (int i = 1; i <= c->rounds; ++i) {
        // acquire 等待发布信号
        while (c->ready.load(std::memory_order_acquire) == 0) {
            sched_yield();
        }
        int v = c->payload;
        if (v <= last) {
            c->errors++;
        }
        last = v;
        c->ready.store(0, std::memory_order_relaxed);
    }
    return NULL;
}

// 函数: run_publish_experiment
// 参数: int rounds
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void run_publish_experiment(int rounds) {
    PublishCtx c;
    memset(&c, 0, sizeof(c));
    c.rounds = rounds;
    c.ready.store(0, std::memory_order_relaxed);
    c.payload = 0;
    c.errors = 0;

    pthread_t tw, tr;
    int64_t begin = now_us();
    pthread_create(&tw, NULL, publish_writer, &c);
    pthread_create(&tr, NULL, publish_reader, &c);
    pthread_join(tw, NULL);
    pthread_join(tr, NULL);
    int64_t end = now_us();

    printf("publish_pattern: rounds=%d errors=%d elapsed=%.2fms\n",
           rounds, c.errors, (end - begin) / 1000.0);
}

// 函数: print_bench_header
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_bench_header() {
    printf("mode            threads loops/thread expected    actual      elapsed(ms) throughput(Mops/s)\n");
    printf("--------------- ------- ------------ ----------- ----------- ---------- -------------------\n");
}

// 函数: print_bench_result
// 参数: const CounterBenchResult& r
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_bench_result(const CounterBenchResult& r) {
    printf("%-15s %7d %12lld %11lld %11lld %10.2f %19.2f\n",
           bench_mode_name(r.mode), r.threads, (long long)r.loops,
           (long long)r.expected, (long long)r.actual, r.elapsed_ms, r.mops);
}

// 函数: arg_or_default
// 参数: int argc, char** argv, int idx, int dft
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) {
        return dft;
    }
    return atoi(argv[idx]);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    // 参数:
    // [1] threads(default 8)
    // [2] loops(default 400000)
    // [3] publish_rounds(default 200000)
    int threads = arg_or_default(argc, argv, 1, 8);
    int loops = arg_or_default(argc, argv, 2, 400000);
    int rounds = arg_or_default(argc, argv, 3, 200000);
    if (threads <= 0) threads = 1;
    if (loops <= 0) loops = 1;
    if (rounds <= 0) rounds = 1;

    printf("config: threads=%d loops=%d publish_rounds=%d\n\n", threads, loops, rounds);

    print_bench_header();
    CounterBenchResult a = run_counter_bench(BENCH_MUTEX, threads, loops);
    CounterBenchResult b = run_counter_bench(BENCH_ATOMIC_RELAXED, threads, loops);
    CounterBenchResult c = run_counter_bench(BENCH_ATOMIC_SEQ_CST, threads, loops);
    print_bench_result(a);
    print_bench_result(b);
    print_bench_result(c);

    printf("\n");
    run_publish_experiment(rounds);

    printf("\n解读:\n");
    printf("1) 三个 counter 模式 actual 都应等于 expected（正确性）。\n");
    printf("2) ATOMIC_RELAXED 常用于统计计数，通常吞吐更高。\n");
    printf("3) release/acquire 是发布数据的常见搭配；仅靠 volatile 不够。\n");
    printf("4) atomic 不能替代复杂临界区锁，涉及多个共享对象时仍要 mutex/rwlock。\n");
    return 0;
}
