#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <deque>
#include <errno.h>
#include <string.h>
#include <vector>

/*
学习地图（建议按 1 -> 2 -> 3 -> 4 顺序跑）:

demo1:
  主题: mutex + condition variable + 有界队列 + 流水线背压
  你会学到:
  1) 为什么生产者/消费者需要 while + cond_wait
  2) close() 的正确语义: 不再生产新数据，但要把队列里旧数据消费完
  3) CAP/ENC/REN 三段流水线怎么传递时间戳算 e2e 延迟

demo2:
  主题: rwlock 读多写少
  你会学到:
  1) 读锁可并发，写锁独占
  2) 为什么“配置结构体”适合 rwlock
  3) 用 read_ops/write_ops 观察读写比例

demo3:
  主题: 条件变量超时等待 + 优雅退出
  你会学到:
  1) timedwait 返回超时不等于系统结束
  2) producer_done + 队列关闭 的组合退出协议
  3) 如何避免消费者永久阻塞

demo4:
  主题: 锁顺序反转风险 + 统一锁序修复
  你会学到:
  1) 多把锁时为什么会死锁
  2) 统一全局锁顺序为什么有效
  3) trylock 作为“风险探针”的用法
*/

/*
阅读顺序建议（按函数）:
1) 先看 BoundedQueue 的 push/pop/pop_timeout/close
2) 再看 run_demo1~run_demo4 如何组织线程
3) 最后看各线程函数中的“等待点”和“退出点”

这份文件里最重要的工程思想:
- 等待条件必须写成 while，不是 if（防伪唤醒）
- close 语义是“停止新增”，不是“清空历史数据”
- 多线程收尾要有统一协议：close + broadcast + join
*/

/*
函数导航（解决“函数名看不懂、参数不知道干啥”）:
1) 时间工具:
   - now_ms():
       返回当前单调时钟毫秒，主要用于统计链路时延。
   - timeout_after_ms(timeout_ms):
       参数 timeout_ms 表示“从现在开始再等多久”。
       返回 pthread_cond_timedwait 需要的“绝对时间点”。
2) 队列核心:
   - BoundedQueue(cap):
       参数 cap 是队列容量上限，决定背压何时触发。
   - push(item):
       参数 item 是要入队的数据；返回 false 代表队列已关闭。
   - pop(out):
       参数 out 是出参指针，成功时写入队头元素。
   - pop_timeout(out, timeout_ms):
       参数 timeout_ms 是本次等待上限；返回值:
         1=拿到数据, 0=超时, -1=队列关闭且为空（可退出）。
   - close():
       不再接收新数据，同时唤醒所有等待线程，配合优雅退出。
3) 四个实验入口:
   - run_demo1()/2()/3()/4():
       每个函数负责配置线程、启动、回收并打印统计结论。
4) 线程函数参数:
   - 线程函数统一接收 void* arg，实际是对应 demo 的上下文结构体地址，
     里面包含共享队列、控制标记和统计字段。
*/

// 单位: 毫秒。用于统计端到端时延。
// 函数: now_ms
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 生成“当前时间 + timeout_ms”的绝对超时时间。
// pthread_cond_timedwait 需要的是绝对时间，不是相对时间。
// 函数: timeout_after_ms
// 参数: int timeout_ms
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static timespec timeout_after_ms(int timeout_ms) {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return ts;
}

template <typename T>
class BoundedQueue {
   public:
    // cap: 队列容量上限。满了后 push 会阻塞。
    // 函数: BoundedQueue
    // 参数: cap(队列容量上限)。
    // 说明: 初始化互斥锁、条件变量和关闭标记。
    explicit BoundedQueue(size_t cap) : cap_(cap), closed_(false) {
        pthread_mutex_init(&mtx_, NULL);  // 固定的初始化语句
        pthread_cond_init(&not_full_, NULL);
        pthread_cond_init(&not_empty_, NULL);
    }

    ~BoundedQueue() {
        pthread_mutex_destroy(&mtx_);
        pthread_cond_destroy(&not_full_);
        pthread_cond_destroy(&not_empty_);
    }

    // 入队状态机:
    // 1) 队列满 -> 等 not_full
    // 2) 队列关闭 -> 返回 false
    // 3) 成功写入 -> signal not_empty
    // 函数: push
    // 参数: const T& item
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    bool push(const T& item) {
        pthread_mutex_lock(&mtx_);
        // 队列满且未关闭 -> 生产者等待消费者消费。
        while (!closed_ && q_.size() >= cap_) {
            pthread_cond_wait(&not_full_, &mtx_);
        }
        // 关闭后禁止继续写入。
        if (closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }
        q_.push_back(item);
        // 写入新元素后，唤醒一个“等数据”的消费者。
        pthread_cond_signal(&not_empty_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    // 出队状态机:
    // 1) 队列空且未关闭 -> 等 not_empty
    // 2) 队列空且已关闭 -> 返回 false(真正结束)
    // 3) 成功取出 -> signal not_full
    // 函数: pop
    // 参数: T* out
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    bool pop(T* out) {
        pthread_mutex_lock(&mtx_);
        // 队列空且未关闭 -> 消费者等待生产者写入。
        while (q_.empty() && !closed_) {
            pthread_cond_wait(&not_empty_, &mtx_);
        }
        // 空且已关闭 -> 没有更多数据可读，返回 false 表示结束。
        if (q_.empty() && closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }
        *out = q_.front();
        q_.pop_front();
        // 读走一个元素后，唤醒一个“等空位”的生产者。
        pthread_cond_signal(&not_full_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    // 带超时的出队:
    // false 可能有两种含义:
    // 1) ETIMEDOUT: 这次等超时(可重试)
    // 2) 队列空且已关闭: 流水线结束(应退出)
    // 函数: pop_timeout
    // 参数: T* out, int timeout_ms
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    bool pop_timeout(T* out, int timeout_ms) {
        pthread_mutex_lock(&mtx_);
        // 带超时版本: 既可等待数据，也可在超时后返回给上层做降级处理。
        while (q_.empty() && !closed_) {
            timespec ts = timeout_after_ms(timeout_ms);
            int rc = pthread_cond_timedwait(&not_empty_, &mtx_, &ts);
            if (rc == ETIMEDOUT) {
                // 超时直接返回 false，不代表队列关闭，只是这次没等到数据。
                pthread_mutex_unlock(&mtx_);
                return false;
            }
        }
        if (q_.empty() && closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }
        *out = q_.front();
        q_.pop_front();
        pthread_cond_signal(&not_full_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    // close 的语义:
    // 1) 后续 push 一律失败
    // 2) 已在队列里的元素仍可被 pop
    // 3) 广播唤醒所有等待线程，避免挂死
    // 函数: close
    // 参数: 
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    void close() {
        pthread_mutex_lock(&mtx_);
        closed_ = true;
        // 广播唤醒所有阻塞线程，避免线程永久挂住。
        pthread_cond_broadcast(&not_full_);
        pthread_cond_broadcast(&not_empty_);
        pthread_mutex_unlock(&mtx_);
    }

   private:
    size_t cap_;
    bool closed_;  // 关闭标记: close 后不再允许 push。
    std::deque<T> q_;
    pthread_mutex_t mtx_;
    pthread_cond_t not_full_;
    pthread_cond_t not_empty_;
};

// =========================
// Demo 1: mutex + cond queue
// =========================
// 场景映射:
// CAP(采集) -> ENC(编码) -> REN(渲染)
// 这是音视频最常见的三级流水线雏形。
//
// 关键观察点:
// 1) 如果 ENC 变慢，CAP 会在 raw_q.push 阻塞（上游被背压）
// 2) 如果 REN 变慢，ENC 会在 pkt_q.push 阻塞（中游被背压）
// 3) e2e 延迟 = 当前时刻 - 最初 capture_ms
struct Frame {
    int id;             // 帧序号
    int64_t capture_ms; // 采集时刻
};

struct Packet {
    int id;             // 包序号(对应帧序号)
    int64_t capture_ms; // 原始采集时刻
    int64_t encode_ms;  // 编码完成时刻
};

struct Demo1Ctx {
    int frames;        // 总帧数
    int cap_ms;        // 采集线程每帧耗时
    int enc_base_ms;   // 编码线程基础耗时
    int ren_base_ms;   // 渲染线程基础耗时
    BoundedQueue<Frame>* raw_q;   // CAP -> ENC（原始帧队列）
    BoundedQueue<Packet>* pkt_q;  // ENC -> REN（编码包队列）
};

// 函数: demo1_capture
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo1_capture(void* arg) {
    Demo1Ctx* ctx = (Demo1Ctx*)arg;
    for (int i = 0; i < ctx->frames; ++i) {  // 生产固定数量帧
        Frame f;
        f.id = i;
        f.capture_ms = now_ms();
        // 如果队列被关闭，push 返回 false，线程结束。
        if (!ctx->raw_q->push(f)) {
            break;
        }
        printf("[CAP] frame=%02d\n", f.id);
        usleep(ctx->cap_ms * 1000);
    }
    ctx->raw_q->close();  // 通知编码线程: 不会再有新帧
    return NULL;
}

// 函数: demo1_encode
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo1_encode(void* arg) {
    Demo1Ctx* ctx = (Demo1Ctx*)arg;
    Frame f;
    // 一直消费 raw_q，直到 pop 返回 false(队列关闭且空)。
    while (ctx->raw_q->pop(&f)) {
        usleep((ctx->enc_base_ms + rand() % 6) * 1000);
        Packet p;
        p.id = f.id;
        p.capture_ms = f.capture_ms;
        p.encode_ms = now_ms();
        // 下游关闭后，编码线程也要尽快退出。
        if (!ctx->pkt_q->push(p)) {
            break;
        }
        printf("[ENC] frame=%02d enc_cost=%lldms\n", p.id,
               (long long)(p.encode_ms - p.capture_ms));
    }
    ctx->pkt_q->close();  // 通知渲染线程结束
    return NULL;
}

// 函数: demo1_render
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo1_render(void* arg) {
    Demo1Ctx* ctx = (Demo1Ctx*)arg;
    Packet p;
    int count = 0;
    int64_t total = 0;
    int64_t max_v = 0;
    while (ctx->pkt_q->pop(&p)) {  // 消费编码后的包并统计端到端时延
        usleep((ctx->ren_base_ms + rand() % 4) * 1000);
        // 用“最初采集时刻”统计端到端延迟，不用 encode_ms，
        // 是为了反映完整链路延迟，而不是某个阶段的局部延迟。
        int64_t e2e = now_ms() - p.capture_ms;
        ++count;
        total += e2e;
        if (e2e > max_v) {
            max_v = e2e;
        }
        printf("[REN] frame=%02d e2e=%lldms\n", p.id, (long long)e2e);
    }
    if (count > 0) {
        printf("demo1 summary: frames=%d avg=%lldms max=%lldms\n", count,
               (long long)(total / count), (long long)max_v);
    }
    return NULL;
}

// 函数: run_demo1
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo1() {
    // 小容量队列用于模拟背压: 让你更容易观察“上游阻塞”。
    BoundedQueue<Frame> raw_q(4);
    BoundedQueue<Packet> pkt_q(4);

    Demo1Ctx ctx;
    // 这里设置 CAP 最慢，方便观察“队列多为空”的情况。
    // 你可以把 enc_base_ms 改大到 25~40ms，再看背压方向如何变化。
    ctx.frames = 20;
    ctx.cap_ms = 30;
    ctx.enc_base_ms = 9;
    ctx.ren_base_ms = 5;
    ctx.raw_q = &raw_q;
    ctx.pkt_q = &pkt_q;

    pthread_t t_cap;
    pthread_t t_enc;
    pthread_t t_ren;
    // 典型三段流水线: capture -> encode -> render
    pthread_create(&t_cap, NULL, demo1_capture, &ctx);
    pthread_create(&t_enc, NULL, demo1_encode, &ctx);
    pthread_create(&t_ren, NULL, demo1_render, &ctx);

    // join 顺序只要覆盖全部线程即可，这里按创建顺序回收。
    pthread_join(t_cap, NULL);
    pthread_join(t_enc, NULL);
    pthread_join(t_ren, NULL);
    return 0;
}

// =========================
// Demo 2: rwlock read-heavy config
// =========================
// 场景映射:
// 一个控制线程周期更新编码参数，多个工作线程频繁读取参数。
// 这是播放器/编码器里典型的“控制面(少写) + 数据面(多读)”模型。
//
// 关键观察点:9
// 1) reads 通常远大于 writes
// 2) bad_reads 应始终为 0（说明读取到了受保护的一致快照）
struct SharedCfg {
    int bitrate_kbps;  // 码率
    int gop;           // I 帧间隔
    int qp;            // 量化参数
    uint64_t version;  // 配置版本
};

struct Demo2Ctx {
    pthread_rwlock_t rw;  // 读写锁: 读并发、写独占
    SharedCfg cfg;                // 配置
    volatile int stop;            // 线程停止标记
    volatile uint64_t read_ops;   // 读次数 其实可以声明为atomic<uint64_t>类型，然后进行自增操作
    volatile uint64_t write_ops;  // 写次数
    volatile uint64_t bad_reads;  // 读到异常值次数(用于 sanity check)
};

// 函数: demo2_reader
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo2_reader(void* arg) {
    Demo2Ctx* ctx = (Demo2Ctx*)arg;
    while (!ctx->stop) {
        // 读锁可并发，多个 reader 可同时进入。
        pthread_rwlock_rdlock(&ctx->rw);
        // 先复制出快照，再解锁，减少锁持有时间。
        SharedCfg snap = ctx->cfg;
        pthread_rwlock_unlock(&ctx->rw);
        if (snap.bitrate_kbps < 1000 || snap.gop < 10 || snap.qp < 10 || snap.qp > 51) {
            __sync_fetch_and_add(&ctx->bad_reads, 1);
        }
        __sync_fetch_and_add(&ctx->read_ops, 1);
    }
    return NULL;
}

// 函数: demo2_writer
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo2_writer(void* arg) {
    Demo2Ctx* ctx = (Demo2Ctx*)arg;
    while (!ctx->stop) {
        // 写锁独占，更新期间 reader 会被阻塞。
        pthread_rwlock_wrlock(&ctx->rw);
        ctx->cfg.bitrate_kbps += 250;
        if (ctx->cfg.bitrate_kbps > 8000) {
            ctx->cfg.bitrate_kbps = 2000;
        }
        ctx->cfg.gop = 25 + (ctx->cfg.version % 3) * 5;
        ctx->cfg.qp = 22 + (ctx->cfg.version % 6);
        ctx->cfg.version++;  // 每次写入版本递增，便于后续扩展一致性检查
        pthread_rwlock_unlock(&ctx->rw);
        __sync_fetch_and_add(&ctx->write_ops, 1);
        usleep(30 * 1000);
    }
    return NULL;
}

// 函数: run_demo2
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo2() {
    Demo2Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    // 读写锁初始化，默认属性即可满足本实验。
    pthread_rwlock_init(&ctx.rw, NULL);
    ctx.cfg.bitrate_kbps = 2000;
    ctx.cfg.gop = 30;
    ctx.cfg.qp = 24;
    ctx.cfg.version = 1;

    const int kReaders = 4;  // 模拟“读多写少”工作负载
    pthread_t readers[kReaders];
    pthread_t writer;

    pthread_create(&writer, NULL, demo2_writer, &ctx);
    for (int i = 0; i < kReaders; ++i) {
        pthread_create(&readers[i], NULL, demo2_reader, &ctx);
    }

    int64_t begin = now_ms();
    usleep(3000 * 1000);  // 运行 3 秒观察吞吐
    ctx.stop = 1;

    pthread_join(writer, NULL);
    for (int i = 0; i < kReaders; ++i) {
        pthread_join(readers[i], NULL);
    }

    pthread_rwlock_destroy(&ctx.rw);
    int64_t elapsed = now_ms() - begin;
    printf("demo2 summary: elapsed=%lldms reads=%llu writes=%llu bad_reads=%llu\n",
           (long long)elapsed, (unsigned long long)ctx.read_ops,
           (unsigned long long)ctx.write_ops, (unsigned long long)ctx.bad_reads);
    return 0;
}

// =========================
// Demo 3: cond timed wait
// =========================
// 场景映射:
// 消费线程不允许无限期阻塞，需要周期唤醒做健康检查或降级处理。
//
// 关键观察点:
// 1) timeout_hits 增加不一定是 bug，可能只是“这段时间没数据”
// 2) produced == consumed 表示没有丢数据
// 3) 正确退出靠 producer_done + close 的组合语义
struct Demo3Ctx {
    BoundedQueue<int>* q;
    volatile int producer_done;  // 生产者结束标记
    int produced;                // 实际生产数量
    int consumed;                // 实际消费数量
    int timeout_hits;            // 消费者超时次数
};

// 函数: demo3_producer
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo3_producer(void* arg) {
    Demo3Ctx* ctx = (Demo3Ctx*)arg;
    for (int i = 0; i < 18; ++i) {  // 以不稳定间隔生产，制造空队列窗口
        if (!ctx->q->push(i)) {
            break;
        }
        __sync_fetch_and_add(&ctx->produced, 1);
        usleep((20 + rand() % 90) * 1000);
    }
    ctx->q->close();
    ctx->producer_done = 1;
    return NULL;
}

// 函数: demo3_consumer
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo3_consumer(void* arg) {
    Demo3Ctx* ctx = (Demo3Ctx*)arg;
    while (1) {
        int v = -1;
        // 最多等 60ms，超时说明当前没有数据可拿。
        if (!ctx->q->pop_timeout(&v, 60)) {
            __sync_fetch_and_add(&ctx->timeout_hits, 1);
            if (ctx->producer_done) {
                // 生产者已结束后再做一次短探测，确认队列确实为空再退出。
                // 做一次短等待二次确认，避免“刚好错过最后一个数据”。
                if (!ctx->q->pop_timeout(&v, 1)) {
                    break;
                }
            } else {
                // 生产者还没结束，继续下一轮等待。
                continue;
            }
        }
        __sync_fetch_and_add(&ctx->consumed, 1);
        printf("[C] got=%d\n", v);
        usleep((15 + rand() % 50) * 1000);
    }
    return NULL;
}

// 函数: run_demo3
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo3() {
    BoundedQueue<int> q(6);
    Demo3Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.q = &q;

    pthread_t p, c;
    // 单生产者 + 单消费者足够演示 timed wait 语义。
    pthread_create(&p, NULL, demo3_producer, &ctx);
    pthread_create(&c, NULL, demo3_consumer, &ctx);
    pthread_join(p, NULL);
    pthread_join(c, NULL);

    printf("demo3 summary: produced=%d consumed=%d timeout_hits=%d\n", ctx.produced,
           ctx.consumed, ctx.timeout_hits);
    return 0;
}

// =========================
// Demo 4: lock-order inversion risk and fix
// =========================
// 场景映射:
// 两个共享资源(A/B)需要成对访问。若不同线程拿锁顺序不一致，容易形成循环等待。
//
// phase1:
// T1: lock(A) -> trylock(B)
// T2: lock(B) -> trylock(A)
// trylock 失败次数作为“锁顺序冲突”的风险指标。
//
// phase2:
// 所有线程统一按同一顺序拿锁(这里按地址排序)。
// 这就是工程里最常见、最稳的多锁死锁规避策略。
struct Demo4Ctx {
    pthread_mutex_t a;
    pthread_mutex_t b;
    volatile int stop;               // 线程停止标记
    volatile uint64_t risk_collisions;  // 风险阶段 trylock 失败次数
    volatile uint64_t safe_ops;         // 修复阶段成功操作次数
};

// 函数: demo4_risky_t1
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo4_risky_t1(void* arg) {
    Demo4Ctx* ctx = (Demo4Ctx*)arg;
    while (!ctx->stop) {
        // T1: 先拿 a 再尝试拿 b
        pthread_mutex_lock(&ctx->a);
        usleep(1000);
        // trylock 失败表示“对方很可能反向持有了另一把锁”。
        if (pthread_mutex_trylock(&ctx->b) != 0) {
            __sync_fetch_and_add(&ctx->risk_collisions, 1);
        } else {
            pthread_mutex_unlock(&ctx->b);
        }
        pthread_mutex_unlock(&ctx->a);
        usleep(1000);
    }
    return NULL;
}

// 函数: demo4_risky_t2
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo4_risky_t2(void* arg) {
    Demo4Ctx* ctx = (Demo4Ctx*)arg;
    while (!ctx->stop) {
        // T2: 先拿 b 再尝试拿 a，与 T1 锁顺序相反 -> 高风险
        pthread_mutex_lock(&ctx->b);
        usleep(1000);
        if (pthread_mutex_trylock(&ctx->a) != 0) {
            __sync_fetch_and_add(&ctx->risk_collisions, 1);
        } else {
            pthread_mutex_unlock(&ctx->a);
        }
        pthread_mutex_unlock(&ctx->b);
        usleep(1000);
    }
    return NULL;
}

// 函数: lock_in_address_order
// 参数: pthread_mutex_t* m1, pthread_mutex_t* m2
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void lock_in_address_order(pthread_mutex_t* m1, pthread_mutex_t* m2) {
    // 修复策略: 全局统一顺序拿锁，避免循环等待。
    if (m1 < m2) {
        pthread_mutex_lock(m1);
        pthread_mutex_lock(m2);
    } else {
        pthread_mutex_lock(m2);
        pthread_mutex_lock(m1);
    }
}

// 函数: unlock_in_address_order
// 参数: pthread_mutex_t* m1, pthread_mutex_t* m2
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void unlock_in_address_order(pthread_mutex_t* m1, pthread_mutex_t* m2) {
    if (m1 < m2) {
        pthread_mutex_unlock(m2);
        pthread_mutex_unlock(m1);
    } else {
        pthread_mutex_unlock(m1);
        pthread_mutex_unlock(m2);
    }
}

// 函数: demo4_safe_worker
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo4_safe_worker(void* arg) {
    Demo4Ctx* ctx = (Demo4Ctx*)arg;
    while (!ctx->stop) {
        // 两个线程都按相同顺序拿锁，不会形成反转。
        lock_in_address_order(&ctx->a, &ctx->b);
        __sync_fetch_and_add(&ctx->safe_ops, 1);
        unlock_in_address_order(&ctx->a, &ctx->b);
        usleep(500);
    }
    return NULL;
}

// 函数: run_demo4
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo4() {
    Demo4Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    // 两把锁用于演示“锁顺序反转”问题。
    pthread_mutex_init(&ctx.a, NULL);
    pthread_mutex_init(&ctx.b, NULL);

    pthread_t t1, t2;
    // phase 1: 演示风险写法(用 trylock 避免程序真死锁)。
    pthread_create(&t1, NULL, demo4_risky_t1, &ctx);
    pthread_create(&t2, NULL, demo4_risky_t2, &ctx);
    usleep(1500 * 1000);
    ctx.stop = 1;
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("demo4 risk-phase: lock-order collisions=%llu\n",
           (unsigned long long)ctx.risk_collisions);

    // phase 2: 切换到“统一锁序”的安全写法。
    ctx.stop = 0;
    pthread_t s1, s2;
    pthread_create(&s1, NULL, demo4_safe_worker, &ctx);
    pthread_create(&s2, NULL, demo4_safe_worker, &ctx);
    usleep(1500 * 1000);
    ctx.stop = 1;
    pthread_join(s1, NULL);
    pthread_join(s2, NULL);
    printf("demo4 safe-phase: safe_ops=%llu (no lock-order inversion)\n",
           (unsigned long long)ctx.safe_ops);

    pthread_mutex_destroy(&ctx.a);
    pthread_mutex_destroy(&ctx.b);
    return 0;
}

// 函数: print_usage
// 参数: const char* bin
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_usage(const char* bin) {
    printf("usage: %s [demo]\n", bin);
    printf("demo:\n");
    printf("  1  mutex queue pipeline (capture->encode->render)\n");
    printf("  2  rwlock read-heavy config benchmark\n");
    printf("  3  cond timed wait queue\n");
    printf("  4  lock-order risk vs fix\n");
    printf("  all  run all demos in sequence\n");
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));
    const char* mode = (argc >= 2) ? argv[1] : "all";

    // 通过参数选择要跑的 demo，便于你按学习路径逐个验证:
    // 1(基础流水线) -> 2(读写锁) -> 3(超时与退出) -> 4(锁顺序)
    if (strcmp(mode, "1") == 0) {
        return run_demo1();
    }
    if (strcmp(mode, "2") == 0) {
        return run_demo2();
    }
    if (strcmp(mode, "3") == 0) {
        return run_demo3();
    }
    if (strcmp(mode, "4") == 0) {
        return run_demo4();
    }
    if (strcmp(mode, "all") == 0) {
        run_demo1();
        run_demo2();
        run_demo3();
        run_demo4();
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
