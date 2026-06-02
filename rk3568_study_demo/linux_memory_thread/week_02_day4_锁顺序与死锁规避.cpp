#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

/*
Week2 Day4: 锁顺序与死锁规避

这个 demo 到底在“验证什么”:
1) 验证假设A:
   当两个线程以相反顺序请求两把锁(A->B 与 B->A)时，
   会出现大量锁顺序冲突，且在真实阻塞锁场景下可演化为死锁。
2) 验证假设B:
   只要把“任意两把锁的加锁顺序”统一（例如地址升序或rank升序），
   冲突会显著下降，循环等待条件被破坏，从而规避死锁。
3) 验证方式:
   本 demo 不让程序真死锁，而是用 trylock 失败次数作为风险代理指标。

为什么设计成两阶段（RISKY vs SAFE）:
1) RISKY阶段:
   让两个线程故意采用反向锁序，观察冲突率 collision_rate。
2) SAFE阶段:
   保持“业务意图”不变（都要同时持有 A 和 B），
   仅替换加锁策略为统一锁序，比较冲突率是否下降到接近 0。
3) 这样设计能直接证明:
   “真正改变系统行为的是锁顺序策略，而不是业务工作量变化”。

为什么用 trylock 而不是 lock:
1) 若风险阶段用阻塞 lock，很可能真卡死，实验无法结束。
2) trylock 失败可计数，可重复跑，适合教学和量化比较。
3) 在工程里，它也常作为“非关键路径的降级策略”。

你应该关注的输出指标:
1) ok_ops:
   成功完成“同时拿到两把锁”的操作次数，反映有效吞吐。
2) collisions:
   第二把锁 trylock 失败次数，反映锁序冲突压力。
3) collision_rate = collisions / (ok_ops + collisions):
   冲突概率，越高说明锁序设计越危险。

和你 SoC 音视频岗位的映射:
1) 解码状态锁 + 缓冲队列锁 + 渲染路径锁，常常成对或成组出现。
2) 若不同模块各自写“先拿谁后拿谁”，很容易出现偶发卡死。
3) “统一锁序（或 lock rank）”是最常用、最可落地的治理方案。

阅读顺序建议:
1) risky_worker_ab / risky_worker_ba（看风险如何制造）
2) lock_ordered / unlock_ordered（看修复策略核心）
3) run_risky_phase / run_safe_phase（看量化结果如何比较）
*/

/*
函数与参数速查:
1) lock_ordered(x, y) / unlock_ordered(x, y):
   - x,y: 任意两把锁指针；函数内部统一按地址排序后再加/解锁。
   - 目的: 无论调用顺序如何，最终实际锁序保持一致。
2) risky_worker_ab(arg) / risky_worker_ba(arg):
   - arg 实际是 Ctx*。
   - AB 与 BA 故意反向，制造锁序冲突压力用于对照。
3) safe_worker_any_order(arg) / safe_worker_reverse_call(arg):
   - 仍然传入不同顺序参数，但实际由 lock_ordered 统一顺序。
4) run_risky_phase(c, runtime_ms) / run_safe_phase(c, runtime_ms):
   - runtime_ms: 每个阶段跑多长时间，便于对比冲突率。
5) collision_rate:
   - collisions/(ok_ops+collisions)，用于量化锁序风险。
*/

struct Stat {
    uint64_t ok_ops;     // 成功拿到两把锁并完成一次操作
    uint64_t collisions; // trylock 失败次数（冲突信号）
};

struct Ctx {
    pthread_mutex_t lock_a; // 共享资源A对应锁
    pthread_mutex_t lock_b; // 共享资源B对应锁
    volatile int stop;      // 控制线程退出的停止标记
    Stat t1;                // 线程1统计
    Stat t2;                // 线程2统计
};

// 函数: risky_worker_ab
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* risky_worker_ab(void* arg) {
    Ctx* c = (Ctx*)arg;
    while (!c->stop) {
        // 风险路径1:
        // 线程1固定顺序 A -> B
        pthread_mutex_lock(&c->lock_a);
        usleep(800);
        // 尝试拿第二把锁:
        // 成功 => 完成一次有效操作
        // 失败 => 记录一次锁序冲突
        if (pthread_mutex_trylock(&c->lock_b) == 0) {
            c->t1.ok_ops++;
            pthread_mutex_unlock(&c->lock_b);
        } else {
            c->t1.collisions++;
        }
        pthread_mutex_unlock(&c->lock_a);
        usleep(200);
    }
    return NULL;
}

// 函数: risky_worker_ba
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* risky_worker_ba(void* arg) {
    Ctx* c = (Ctx*)arg;
    while (!c->stop) {
        // 风险路径2:
        // 线程2故意反向顺序 B -> A（与线程1形成顺序反转）
        pthread_mutex_lock(&c->lock_b);
        usleep(800);
        if (pthread_mutex_trylock(&c->lock_a) == 0) {
            c->t2.ok_ops++;
            pthread_mutex_unlock(&c->lock_a);
        } else {
            c->t2.collisions++;
        }
        pthread_mutex_unlock(&c->lock_b);
        usleep(200);
    }
    return NULL;
}

// 函数: lock_ordered
// 参数: pthread_mutex_t* x, pthread_mutex_t* y
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void lock_ordered(pthread_mutex_t* x, pthread_mutex_t* y) {
    // 修复核心:
    // 对任意两把锁，先计算全局一致顺序（这里用地址升序）再加锁。
    //
    // 关键点:
    // - 调用者传 (A,B) 或 (B,A) 都没关系
    // - 最终实际加锁顺序永远一致
    // - 因为“等待图”单向化，循环等待条件被破坏
    //
    // 注意:
    // 地址排序是“方便实现”的方法；生产系统更推荐 lock rank 枚举。
    if (x < y) {
        pthread_mutex_lock(x);
        pthread_mutex_lock(y);
    } else {
        pthread_mutex_lock(y);
        pthread_mutex_lock(x);
    }
}

// 函数: unlock_ordered
// 参数: pthread_mutex_t* x, pthread_mutex_t* y
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void unlock_ordered(pthread_mutex_t* x, pthread_mutex_t* y) {
    // 解锁按与加锁相反顺序，保持锁语义清晰，降低误用概率。
    if (x < y) {
        pthread_mutex_unlock(y);
        pthread_mutex_unlock(x);
    } else {
        pthread_mutex_unlock(x);
        pthread_mutex_unlock(y);
    }
}

// 函数: safe_worker_any_order
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* safe_worker_any_order(void* arg) {
    Ctx* c = (Ctx*)arg;
    while (!c->stop) {
        // 业务代码写法1: 传参是 (A, B)
        // 但内部会被归一化为统一顺序。
        lock_ordered(&c->lock_a, &c->lock_b);
        c->t1.ok_ops++;
        unlock_ordered(&c->lock_a, &c->lock_b);
        usleep(200);
    }
    return NULL;
}

// 函数: safe_worker_reverse_call
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* safe_worker_reverse_call(void* arg) {
    Ctx* c = (Ctx*)arg;
    while (!c->stop) {
        // 业务代码写法2: 故意传 (B, A)
        // 仍然会被 lock_ordered 归一到与上面一致的顺序。
        lock_ordered(&c->lock_b, &c->lock_a);
        c->t2.ok_ops++;
        unlock_ordered(&c->lock_b, &c->lock_a);
        usleep(200);
    }
    return NULL;
}

// 函数: reset_stats
// 参数: Ctx* c
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void reset_stats(Ctx* c) {
    // 每阶段开始前清零统计，保证阶段间对比公平。
    memset(&c->t1, 0, sizeof(c->t1));
    memset(&c->t2, 0, sizeof(c->t2));
}

// 函数: run_risky_phase
// 参数: Ctx* c, int runtime_ms
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void run_risky_phase(Ctx* c, int runtime_ms) {
    // 目标:
    // 用“反向锁序”制造高冲突场景，观察风险基线。
    reset_stats(c);
    c->stop = 0;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, risky_worker_ab, c);
    pthread_create(&t2, NULL, risky_worker_ba, c);
    usleep(runtime_ms * 1000);
    c->stop = 1;
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    uint64_t ok = c->t1.ok_ops + c->t2.ok_ops;
    uint64_t col = c->t1.collisions + c->t2.collisions;
    printf("[RISKY] ok_ops=%llu collisions=%llu collision_rate=%.2f%%\n",
           (unsigned long long)ok, (unsigned long long)col,
           (ok + col > 0) ? (100.0 * col / (double)(ok + col)) : 0.0);
}

// 函数: run_safe_phase
// 参数: Ctx* c, int runtime_ms
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void run_safe_phase(Ctx* c, int runtime_ms) {
    // 目标:
    // 仅替换加锁策略（统一锁序），观察冲突是否显著下降。
    reset_stats(c);
    c->stop = 0;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, safe_worker_any_order, c);
    pthread_create(&t2, NULL, safe_worker_reverse_call, c);
    usleep(runtime_ms * 1000);
    c->stop = 1;
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    uint64_t ok = c->t1.ok_ops + c->t2.ok_ops;
    uint64_t col = c->t1.collisions + c->t2.collisions;
    printf("[SAFE ] ok_ops=%llu collisions=%llu collision_rate=%.2f%%\n",
           (unsigned long long)ok, (unsigned long long)col,
           (ok + col > 0) ? (100.0 * col / (double)(ok + col)) : 0.0);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    // 参数:
    // [1] 每阶段运行时长 ms (default 1500)
    int runtime_ms = (argc > 1) ? atoi(argv[1]) : 1500;
    if (runtime_ms <= 0) {
        runtime_ms = 1000;
    }

    Ctx c;
    memset(&c, 0, sizeof(c));
    pthread_mutex_init(&c.lock_a, NULL);
    pthread_mutex_init(&c.lock_b, NULL);

    printf("config: runtime_per_phase=%dms\n", runtime_ms);
    // 顺序固定为:
    // 1) 先跑风险阶段拿到“坏例子基线”
    // 2) 再跑修复阶段看改进幅度
    run_risky_phase(&c, runtime_ms);
    run_safe_phase(&c, runtime_ms);

    pthread_mutex_destroy(&c.lock_a);
    pthread_mutex_destroy(&c.lock_b);

    printf("\n解读:\n");
    printf("1) 风险阶段 collision_rate 通常明显更高，代表锁序冲突频繁。\n");
    printf("2) 安全阶段通过统一锁序，将“反向持锁”路径归一化。\n");
    printf("3) 真实项目建议用 lock rank(编号) 代替地址比较，可读性更强。\n");
    printf("4) 这个 demo 验证的是“策略有效性”，不是追求某个绝对吞吐数字。\n");
    return 0;
}
