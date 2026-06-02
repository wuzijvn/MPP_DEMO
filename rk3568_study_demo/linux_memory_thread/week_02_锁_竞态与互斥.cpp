#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>

/*
Week2 Day1/Day5 核心实验: 竞态、互斥锁、原子操作对照

你要学的不是“会写锁”，而是三件事:
1) 识别 race condition 是怎么发生的
2) 理解 mutex/atomic 分别解决什么问题
3) 用数据验证: 正确性和性能是两条轴，不能只看其中一条

本文件支持三种模式:
- NO_LOCK:
    故意不加同步，演示丢计数（data race）
- MUTEX:
    每次 ++ 都进互斥锁，正确但锁竞争开销大
- ATOMIC_RELAXED:
    用 atomic fetch_add(relaxed)，正确且常用于统计计数

推荐运行:
1) g++ -std=c++11 -O2 -pthread week_02_锁_竞态与互斥.cpp -o week_02_lock_lab
2) ./week_02_lock_lab
3) ./week_02_lock_lab 8 800000 5

TSAN（抓竞态）:
1) g++ -std=c++11 -O1 -g -fsanitize=thread -fno-omit-frame-pointer -pthread week_02_锁_竞态与互斥.cpp -o week_02_lock_lab_tsan
2) ./week_02_lock_lab_tsan
*/

/*
函数与参数速查:
1) run_case(mode, thread_num, loops, inject_yield_every):
   - mode: 选择 NO_LOCK / MUTEX / ATOMIC_RELAXED。
   - thread_num: 并发线程数，决定竞争强度。
   - loops: 每线程执行多少次 ++，决定总操作量。
   - inject_yield_every: 每 N 次主动让出 CPU，用于放大竞态窗口。
2) worker(arg):
   - arg 实际类型是 CounterCtx*，包含共享计数器和同步原语。
   - 核心逻辑是“按 mode 切换同步策略并执行同样的 ++ 工作量”。
3) maybe_yield(ctx, i):
   - i 是循环计数；当满足 i%N==0 时调用 sched_yield，制造切换。
4) 输出字段理解:
   - expected: 理论值=threads*loops
   - actual: 实际计数结果
   - lost_updates: 丢失更新数量（仅 NO_LOCK 常见 >0）
   - throughput_mops: 吞吐（百万次/秒）
*/

enum CounterMode {
    MODE_NO_LOCK = 0,
    MODE_MUTEX = 1,
    MODE_ATOMIC_RELAXED = 2,
};

struct CounterCtx {
    CounterMode mode;             // 当前测试模式
    int thread_num;               // 线程数
    int64_t iterations_per_thread;// 每线程循环次数
    int inject_yield_every;       // 每 N 次主动 yield 一次，扩大竞态窗口

    // 普通计数器：NO_LOCK / MUTEX 使用这个
    int64_t plain_counter;
    // 原子计数器：ATOMIC 模式使用这个
    std::atomic<int64_t> atomic_counter;

    pthread_mutex_t mtx;          // mutex 模式使用
    pthread_barrier_t start_barrier; // 让所有线程尽量同时开跑，减少启动偏差
};

struct RunResult {
    CounterMode mode;
    int thread_num;
    int64_t iterations_per_thread;
    int64_t expected;
    int64_t actual;
    int64_t lost_updates;
    double elapsed_ms;
    double throughput_mops; // Million Operations Per Second
};

// 函数: now_us
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t now_us() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// 函数: mode_name
// 参数: CounterMode m
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static const char* mode_name(CounterMode m) {
    switch (m) {
        case MODE_NO_LOCK:
            return "NO_LOCK";
        case MODE_MUTEX:
            return "MUTEX";
        case MODE_ATOMIC_RELAXED:
            return "ATOMIC_RELAXED";
        default:
            return "UNKNOWN";
    }
}

// 函数: maybe_yield
// 参数: CounterCtx* ctx, int64_t i
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void maybe_yield(CounterCtx* ctx, int64_t i) {
    // 作用:
    // 主动制造线程切换，让 NO_LOCK 更容易暴露丢计数。
    if (ctx->inject_yield_every > 0 && (i % ctx->inject_yield_every) == 0) {
        sched_yield();
    }
}

// 函数: worker
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* worker(void* arg) {
    CounterCtx* ctx = (CounterCtx*)arg;

    // 等所有线程就位后再开始，避免“前几个线程先跑太多”导致结果抖动。
    pthread_barrier_wait(&ctx->start_barrier);

    for (int64_t i = 0; i < ctx->iterations_per_thread; ++i) {
        if (ctx->mode == MODE_NO_LOCK) {
            // 这是故意错误写法，用于演示 data race。
            ctx->plain_counter++;
        } else if (ctx->mode == MODE_MUTEX) {
            // 正确但重：每次更新都要 lock/unlock。
            pthread_mutex_lock(&ctx->mtx);
            ctx->plain_counter++;
            pthread_mutex_unlock(&ctx->mtx);
        } else {
            // 正确且轻：统计计数通常用 relaxed 即可。
            // 因为这里只关心“原子性”，不依赖跨线程先后顺序。
            ctx->atomic_counter.fetch_add(1, std::memory_order_relaxed);
        }
        maybe_yield(ctx, i);
    }
    return NULL;
}

// 函数: run_case
// 参数: CounterMode mode, int thread_num, int64_t loops, int inject_yield_every
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static RunResult run_case(CounterMode mode, int thread_num, int64_t loops, int inject_yield_every) {
    CounterCtx ctx;
    ctx.mode = mode;
    ctx.thread_num = thread_num;
    ctx.iterations_per_thread = loops;
    ctx.inject_yield_every = inject_yield_every;
    ctx.plain_counter = 0;
    ctx.atomic_counter.store(0, std::memory_order_relaxed);
    pthread_mutex_init(&ctx.mtx, NULL);
    pthread_barrier_init(&ctx.start_barrier, NULL, (unsigned int)thread_num);

    // 固定上限，避免用户输入过大时越界。
    pthread_t tids[128];
    if (thread_num > 128) {
        thread_num = 128;
        ctx.thread_num = 128;
    }

    // 时间统计覆盖“创建+执行+回收”整个并发阶段。
    int64_t begin = now_us();
    for (int i = 0; i < thread_num; ++i) {
        pthread_create(&tids[i], NULL, worker, &ctx);
    }
    for (int i = 0; i < thread_num; ++i) {
        pthread_join(tids[i], NULL);
    }
    int64_t end = now_us();

    RunResult r;
    r.mode = mode;
    r.thread_num = thread_num;
    r.iterations_per_thread = loops;
    r.expected = (int64_t)thread_num * loops;
    r.actual = (mode == MODE_ATOMIC_RELAXED)
                   ? ctx.atomic_counter.load(std::memory_order_relaxed)
                   : ctx.plain_counter;
    r.lost_updates = r.expected - r.actual;
    r.elapsed_ms = (end - begin) / 1000.0;
    r.throughput_mops = (r.expected / 1000000.0) / (r.elapsed_ms / 1000.0);

    pthread_barrier_destroy(&ctx.start_barrier);
    pthread_mutex_destroy(&ctx.mtx);
    return r;
}

// 函数: print_header
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_header() {
    printf("mode            threads loops/thread expected    actual      lost_updates elapsed(ms) throughput(Mops/s)\n");
    printf("--------------- ------- ------------ ----------- ----------- ------------ ---------- -------------------\n");
}

// 函数: print_result
// 参数: const RunResult& r
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_result(const RunResult& r) {
    printf("%-15s %7d %12lld %11lld %11lld %12lld %10.2f %19.2f\n",
           mode_name(r.mode), r.thread_num, (long long)r.iterations_per_thread,
           (long long)r.expected, (long long)r.actual, (long long)r.lost_updates,
           r.elapsed_ms, r.throughput_mops);
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
    // [1] 线程数(default 8)
    // [2] 每线程循环(default 500000)
    // [3] 重复次数(default 3)
    // [4] 每 N 次注入一次 sched_yield(default 128)
    int threads = arg_or_default(argc, argv, 1, 8);
    int loops = arg_or_default(argc, argv, 2, 500000);
    int rounds = arg_or_default(argc, argv, 3, 3);
    int yield_every = arg_or_default(argc, argv, 4, 128);

    if (threads <= 0) {
        threads = 1;
    }
    if (loops <= 0) {
        loops = 1;
    }
    if (rounds <= 0) {
        rounds = 1;
    }
    if (yield_every < 0) {
        yield_every = 0;
    }

    printf("config: threads=%d loops/thread=%d rounds=%d yield_every=%d\n",
           threads, loops, rounds, yield_every);
    printf("note: NO_LOCK 是故意错误写法，用来观察竞态，不可用于生产代码。\n\n");

    // 每轮都跑三种模式，方便横向比较。
    print_header();
    for (int i = 0; i < rounds; ++i) {
        RunResult n = run_case(MODE_NO_LOCK, threads, loops, yield_every);
        RunResult m = run_case(MODE_MUTEX, threads, loops, yield_every);
        RunResult a = run_case(MODE_ATOMIC_RELAXED, threads, loops, yield_every);
        print_result(n);
        print_result(m);
        print_result(a);
        if (i != rounds - 1) {
            printf("----------------------------------------------------------------------------------------------------------\n");
        }
    }

    printf("\n如何解读:\n");
    printf("1) NO_LOCK 的 lost_updates 通常 > 0，说明发生 data race。\n");
    printf("2) MUTEX/ATOMIC 的 lost_updates 应为 0（正确性通过）。\n");
    printf("3) ATOMIC_RELAXED 常比 MUTEX 快，尤其在仅做计数统计时。\n");
    printf("4) 但 atomic 不能替代所有锁：复杂临界区仍需 mutex/rwlock。\n");
    return 0;
}
