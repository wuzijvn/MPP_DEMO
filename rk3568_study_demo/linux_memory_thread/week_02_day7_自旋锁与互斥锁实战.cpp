#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atomic>

/*
Week2 Day7: 自旋锁 vs 互斥锁（面向 SoC 音视频岗位）

你面试被问“自旋锁”的核心，其实不在 API 本身，而在“何时用”:
1) 自旋锁(pthread_spinlock):
   - 等锁时不会睡眠，持续占用 CPU 轮询
   - 适合: 临界区极短、线程不会睡眠、上下文切换成本高的场景
   - 不适合: 临界区较长或高竞争（会白白烧 CPU）
2) 互斥锁(pthread_mutex):
   - 竞争时可让线程睡眠，减少 CPU 空转
   - 适合: 普通用户态业务、大多数多线程共享数据保护

这个 demo 验证的不是“谁绝对更快”，而是:
- 临界区短时：spin 可能更快
- 临界区长/竞争高时：mutex 通常更稳，spin 的 CPU 开销会飙升

运行:
  g++ -std=c++11 -O2 -pthread week_02_day7_自旋锁与互斥锁实战.cpp -o week_02_day7_spin_vs_mutex
  ./week_02_day7_spin_vs_mutex
  ./week_02_day7_spin_vs_mutex 8 200000 3
参数:
  [1] 线程数(default 8)
  [2] 每线程循环数(default 200000)
  [3] 场景数(default 3) -> 自动选取 0us/10us/50us 临界区占用

这个 demo 的“实验设计意图”:
1) 控制变量:
   - 同一场景下，mutex 与 spin 使用完全相同的线程数、循环数、临界区工作量
   - 临界区工作量用 hold_us 模拟，可切换短/中/长
2) 观测变量:
   - wall_ms: 用户真实感知延迟（从开始到结束）
   - cpu_ms : 进程总 CPU 时间（多核会累加，可能大于 wall_ms）
   - throughput_mops: 单位时间完成操作数
   - avg_wait_ns_per_op: 每次加锁平均等待成本
3) 要验证的结论:
   - 临界区短且竞争可控时，spin 可能有优势
   - 临界区变长/竞争加重时，spin 更容易出现“空转烧 CPU”

读结果时的注意事项（防误判）:
1) 不是“哪个永远快”，而是“哪个更适合当前临界区和竞争度”
2) cpu_ms 反映资源消耗，不等于用户等待时间；wall_ms 更接近用户体验
3) 吞吐高不代表策略更优，还要看 cpu_ms 和可持续性（功耗/发热/调度压力）

与 SoC 音视频岗位的直接映射:
1) 用户态主流程（播放器/录屏工具）通常优先 mutex/rwlock
2) 极短临界区、强低延迟路径可考虑 spin，但要严格受控
3) 若出现 CPU 异常升高，要优先排查是否存在“高竞争 + 自旋等待”
*/

/*
函数与参数速查:
1) run_once(kind, threads, loops, hold_us):
   - kind: LOCK_MUTEX 或 LOCK_SPIN。
   - threads: 并发线程数。
   - loops: 每线程操作次数。
   - hold_us: 临界区模拟工作时长（越大竞争越重）。
2) worker(arg):
   - arg 实际是 BenchCtx*，在同一 workload 下切换锁实现。
3) busy_work_us(us):
   - us: 忙等时长，模拟“持锁执行计算”，用于观察锁竞争放大效应。
4) lock_ctx/unlock_ctx:
   - 按 kind 选择对应 pthread_mutex 或 pthread_spin API。
5) 输出指标:
   - wall_ms: 用户感知总耗时。
   - cpu_ms: 总 CPU 消耗（可大于 wall_ms）。
   - avg_wait_ns_per_op: 平均每次加锁等待成本。
*/

enum LockKind {
    LOCK_MUTEX = 0,
    LOCK_SPIN = 1,
};

struct BenchCtx {
    LockKind kind;            // 当前锁类型
    int thread_num;           // 并发线程数
    int64_t loops_per_thread; // 每个线程的操作次数
    int hold_us;              // 临界区内“工作时长”(微秒)，用于模拟临界区长度

    int64_t counter;          // 共享计数器（受锁保护）
    pthread_mutex_t mtx;      // mutex 句柄
    pthread_spinlock_t sp;    // spinlock 句柄
    pthread_barrier_t barrier;// 起跑屏障，减少启动抖动

    std::atomic<int64_t> total_wait_ns; // 累计加锁等待时长（近似）
};

struct BenchResult {
    LockKind kind;
    int threads;
    int64_t loops_per_thread;
    int hold_us;
    int64_t expected;
    int64_t actual;
    double wall_ms;             // 墙钟时间（真实经过时间）
    double cpu_ms;              // 进程CPU时间（所有线程累计）
    double throughput_mops;     // 吞吐（百万次操作/秒）
    double avg_wait_ns_per_op;  // 单次操作平均等待锁时间
};

// 单调时钟：适合性能计时，不受系统时间跳变影响
// 函数: mono_ns
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t mono_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// 进程 CPU 时间：所有线程在 CPU 上消耗时间的总和
// 函数: proc_cpu_ns
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t proc_cpu_ns() {
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// 函数: busy_work_us
// 参数: int us
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void busy_work_us(int us) {
    // 这里不用 usleep，是为了模拟“持锁执行计算”的 CPU 临界区。
    // usleep 会让出 CPU，不符合自旋锁高频临界区讨论场景。
    if (us <= 0) {
        return;
    }
    int64_t begin = mono_ns();
    int64_t target = (int64_t)us * 1000LL;
    while (mono_ns() - begin < target) {
        // busy spin
    }
}

// 统一封装加锁，便于切换 lock 策略做对照实验
// 函数: lock_ctx
// 参数: BenchCtx* c
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static inline void lock_ctx(BenchCtx* c) {
    if (c->kind == LOCK_MUTEX) {
        pthread_mutex_lock(&c->mtx);
    } else {
        pthread_spin_lock(&c->sp);
    }
}

// 统一封装解锁，保持对称
// 函数: unlock_ctx
// 参数: BenchCtx* c
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static inline void unlock_ctx(BenchCtx* c) {
    if (c->kind == LOCK_MUTEX) {
        pthread_mutex_unlock(&c->mtx);
    } else {
        pthread_spin_unlock(&c->sp);
    }
}

// 函数: worker
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* worker(void* arg) {
    BenchCtx* c = (BenchCtx*)arg;

    // 让所有线程尽量同时开始，减少“先启动线程优势”带来的偏差
    pthread_barrier_wait(&c->barrier);

    int64_t local_wait_ns = 0;
    for (int64_t i = 0; i < c->loops_per_thread; ++i) {
        // 只测“获取锁”等待时间，不把临界区执行时间混进去
        int64_t w0 = mono_ns();
        lock_ctx(c);
        local_wait_ns += (mono_ns() - w0);

        // 临界区内容：共享计数 + 模拟持锁计算
        c->counter++;
        busy_work_us(c->hold_us);

        unlock_ctx(c);
    }
    // 线程本地统计累加到全局，避免每轮都做原子加造成额外干扰
    c->total_wait_ns.fetch_add(local_wait_ns, std::memory_order_relaxed);
    return NULL;
}

// 函数: kind_name
// 参数: LockKind k
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static const char* kind_name(LockKind k) {
    return (k == LOCK_MUTEX) ? "MUTEX" : "SPIN";
}

// 函数: run_once
// 参数: LockKind kind, int threads, int64_t loops, int hold_us
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static BenchResult run_once(LockKind kind, int threads, int64_t loops, int hold_us) {
    BenchCtx c;
    memset(&c, 0, sizeof(c));
    c.kind = kind;
    c.thread_num = threads;
    c.loops_per_thread = loops;
    c.hold_us = hold_us;
    c.counter = 0;
    c.total_wait_ns.store(0, std::memory_order_relaxed);
    pthread_mutex_init(&c.mtx, NULL);
    pthread_spin_init(&c.sp, PTHREAD_PROCESS_PRIVATE);
    pthread_barrier_init(&c.barrier, NULL, (unsigned int)threads);

    if (threads > 128) {
        threads = 128;
        c.thread_num = 128;
    }
    pthread_t tids[128];

    // 开始计时：wall + cpu 双视角
    int64_t wall_begin = mono_ns();
    int64_t cpu_begin = proc_cpu_ns();
    for (int i = 0; i < threads; ++i) {
        pthread_create(&tids[i], NULL, worker, &c);
    }
    for (int i = 0; i < threads; ++i) {
        pthread_join(tids[i], NULL);
    }
    int64_t cpu_end = proc_cpu_ns();
    int64_t wall_end = mono_ns();

    BenchResult r;
    r.kind = kind;
    r.threads = threads;
    r.loops_per_thread = loops;
    r.hold_us = hold_us;
    r.expected = (int64_t)threads * loops;
    r.actual = c.counter;
    r.wall_ms = (wall_end - wall_begin) / 1000000.0;
    r.cpu_ms = (cpu_end - cpu_begin) / 1000000.0;
    r.throughput_mops = (r.expected / 1000000.0) / (r.wall_ms / 1000.0);
    r.avg_wait_ns_per_op = (double)c.total_wait_ns.load(std::memory_order_relaxed) / r.expected;

    pthread_barrier_destroy(&c.barrier);
    pthread_spin_destroy(&c.sp);
    pthread_mutex_destroy(&c.mtx);
    return r;
}

// 函数: print_header
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_header() {
    printf("kind   threads loops/thread hold_us expected    actual      wall(ms) cpu(ms) throughput(Mops/s) avg_wait(ns/op)\n");
    printf("-----  ------- ------------ ------- ----------- ----------- -------- -------- ----------------- ---------------\n");
}

// 函数: print_row
// 参数: const BenchResult& r
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_row(const BenchResult& r) {
    printf("%-5s %7d %12lld %7d %11lld %11lld %8.2f %7.2f %17.2f %15.2f\n",
           kind_name(r.kind), r.threads, (long long)r.loops_per_thread, r.hold_us,
           (long long)r.expected, (long long)r.actual, r.wall_ms, r.cpu_ms,
           r.throughput_mops, r.avg_wait_ns_per_op);
}

// 函数: arg_or_default
// 参数: int argc, char** argv, int idx, int dft
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) return dft;
    return atoi(argv[idx]);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    int threads = arg_or_default(argc, argv, 1, 8);
    int loops = arg_or_default(argc, argv, 2, 200000);
    int scenarios = arg_or_default(argc, argv, 3, 3);
    if (threads <= 0) threads = 1;
    if (loops <= 0) loops = 1;
    if (scenarios <= 0) scenarios = 1;

    // 默认场景：短、中、长临界区
    // 0us  : 极短临界区
    // 10us : 中等临界区
    // 50us : 偏长临界区（更容易放大自旋代价）
    int holds[3] = {0, 10, 50};
    if (scenarios > 3) scenarios = 3;

    printf("config: threads=%d loops/thread=%d scenarios=%d\n", threads, loops, scenarios);
    print_header();
    for (int i = 0; i < scenarios; ++i) {
        int hold = holds[i];
        // 同一 hold 下先跑 mutex 再跑 spin，便于同场景横向比较
        BenchResult a = run_once(LOCK_MUTEX, threads, loops, hold);
        BenchResult b = run_once(LOCK_SPIN, threads, loops, hold);
        print_row(a);
        print_row(b);
        if (i != scenarios - 1) {
            printf("---------------------------------------------------------------------------------------------------------------\n");
        }
    }

    printf("\n解读建议:\n");
    printf("1) hold_us 越大，SPIN 的 cpu_ms 往往增长更明显（空转等待增多）。\n");
    printf("2) 若临界区很短且竞争可控，SPIN 可能有优势；否则 MUTEX 通常更稳。\n");
    printf("3) 音视频用户态主流程优先 MUTEX/RWLOCK，SPIN 常见于特定低延迟小临界区。\n");
    printf("4) 若看到 wall_ms 差不多但 SPIN 的 cpu_ms 明显更高，通常意味着“以功耗换时间”。\n");
    printf("5) 真机调优时应联合看 perf/功耗/温度，避免单指标最优导致系统不可持续。\n");
    return 0;
}
