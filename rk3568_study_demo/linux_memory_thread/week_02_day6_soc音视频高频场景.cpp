#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <string.h>
#include <vector>

/*
Week2 Day6: SoC 音视频编解码高频场景综合实验

这个文件不是“语法练习”，是把你入职后最常见的问题抽成可复现实验：

Demo 1: buffer_lifecycle
  关键词: 固定缓冲池 / 背压 / 生命周期 / 资源泄漏检查
  场景映射:
    摄像头采集 -> 编码 -> 显示/输出
    类似 dmabuf/帧缓冲在多个模块间流转
  学习重点:
    1) 固定池如何防止频繁 malloc/free
    2) 哪段慢就会把背压传到上游
    3) 如何做“收尾一致性检查”（是否有 buffer 没归还）

Demo 2: live_policy
  关键词: 低延迟策略 / 队列满时处理 / 丢帧 vs 阻塞
  场景映射:
    实时预览、低延迟直播中，队列满时到底“卡住等”还是“丢旧帧”
  学习重点:
    1) BLOCK 策略：保完整、但延迟可能不断累积
    2) DROP_OLDEST 策略：牺牲完整性、换更低延迟

Demo 3: dynamic_rc
  关键词: 运行时码控参数更新 / 读多写少 / 生效时机
  场景映射:
    控制线程动态调 bitrate/gop/qp，编码线程高频读取
  学习重点:
    1) rwlock 的典型使用场景
    2) 参数并非每帧都能立刻生效（通常关键帧边界更安全）

阅读顺序:
1) 先跑 live_policy，看“低延迟 vs 完整性”的核心权衡
2) 再跑 buffer_lifecycle，看缓冲池复用和背压方向
3) 最后跑 dynamic_rc，看控制面参数更新与数据面生效边界

总使用方式:
  ./week_02_day6_soc_demo buffer_lifecycle [frames] [capture_ms] [encode_ms] [display_ms] [pool]
  ./week_02_day6_soc_demo live_policy [frames] [capture_interval_ms] [encode_ms] [queue_cap]
  ./week_02_day6_soc_demo dynamic_rc [frames] [encode_ms] [ctrl_period_ms]

建议学习顺序:
1) 先理解“吞吐、延迟、完整性”的三角关系（Demo2 最明显）
2) 再理解“缓冲生命周期正确性”是性能优化前提（Demo1）
3) 最后理解“控制面参数更新 ≠ 数据面立刻生效”（Demo3）
*/

/*
函数与参数速查:
1) arg_or_default(argc, argv, idx, dft):
   - idx: 参数下标；dft: 缺失时默认值。
2) Demo1(buffer_lifecycle):
   - run_demo1(argc, argv):
       argv[2..] 分别是 frames/capture_ms/encode_ms/display_ms/pool。
   - demo1_capture_thread/demo1_encode_thread/demo1_display_thread(arg):
       arg 实际类型 Demo1Ctx*，维护缓冲状态机与统计。
   - find_state/count_not_free:
       用于查找指定状态缓冲和收尾泄漏检查。
3) Demo2(live_policy):
   - run_demo2_case(policy, frames, cap_interval_ms, enc_ms, cap):
       policy 决定队列满时 BLOCK 还是 DROP_OLDEST。
   - demo2_capture/demo2_encode(arg):
       arg 是 Demo2Ctx*，核心关注延迟与丢帧的权衡。
4) Demo3(dynamic_rc):
   - run_demo3(argc, argv):
       argv[2..] 是 frames/encode_ms/ctrl_period_ms。
   - demo3_control/demo3_encode(arg):
       arg 是 Demo3Ctx*，演示“控制面写参数 + 数据面按时机生效”。
*/

// 单调时钟，适合测性能（不会被系统时间跳变影响）
// 函数: now_us
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t now_us() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// 函数: now_ms
// 参数: 无。
// 说明: 返回毫秒级时间，常用于阶段日志和统计输出。
static int64_t now_ms() { return now_us() / 1000; }

// 读命令行参数工具:
// - idx 超界时返回默认值 dft
// - 让 main 的参数解析更简洁
// 函数: arg_or_default
// 参数: int argc, char** argv, int idx, int dft
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) {
        return dft;
    }
    return atoi(argv[idx]);
}

// ==========================================================
// Demo 1: 固定缓冲池生命周期（采集 -> 编码 -> 显示 -> 回收）
// ==========================================================
//
// 线程关系:
//   capture_thread: 产生帧，申请 FREE 缓冲 -> 填充 -> 入编码队列
//   encode_thread : 消费编码队列，模拟编码 -> 入输出队列
//   display_thread: 消费输出队列，模拟显示 -> 归还 FREE
//
// 状态机:
//   FREE -> CAPTURED -> QUEUED_ENC -> ENCODING -> QUEUED_OUT -> DISPLAYING -> FREE
//
// 这个 demo 想验证:
// 1) 生命周期闭环是否完整（最后应无泄漏）
// 2) 背压方向如何传播（下游慢会让上游等待）
// 3) 固定池复用如何工作（无运行时 malloc/free）

enum BufState {
    BUF_FREE = 0,
    BUF_CAPTURED = 1,
    BUF_QUEUED_ENC = 2,
    BUF_ENCODING = 3,
    BUF_QUEUED_OUT = 4,
    BUF_DISPLAYING = 5,
};

struct Buffer {
    int id;         // 缓冲编号
    int frame_id;   // 当前绑定的帧号
    BufState state; // 生命周期状态
};

struct Demo1Ctx {
    int total_frames; // 本轮要处理的总帧数
    int capture_ms;   // capture 阶段单帧耗时
    int encode_ms;    // encode 阶段单帧耗时
    int display_ms;   // display 阶段单帧耗时

    volatile int cap_done; // capture 线程是否已完成
    int produced;          // 进入编码队列的帧数
    int encoded;           // 完成编码的帧数
    int displayed;         // 完成显示并归还的帧数

    int64_t cap_wait_us; // capture 等待 FREE 的时间
    int64_t enc_wait_us; // encode 等待 QUEUED_ENC 的时间
    int64_t dsp_wait_us; // display 等待 QUEUED_OUT 的时间

    std::vector<Buffer> bufs; // 固定缓冲池本体
    pthread_mutex_t mtx;      // 保护状态机和计数
    pthread_cond_t cond;      // 状态变化通知
};

// 在缓冲池中查找“第一个处于指定状态”的缓冲索引。
// 返回 -1 表示没找到（线程应等待）。
// 函数: find_state
// 参数: Demo1Ctx* c, BufState s
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int find_state(Demo1Ctx* c, BufState s) {
    for (size_t i = 0; i < c->bufs.size(); ++i) {
        if (c->bufs[i].state == s) {
            return (int)i;
        }
    }
    return -1;
}

// 函数: count_not_free
// 参数: Demo1Ctx* c
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int count_not_free(Demo1Ctx* c) {
    int n = 0;
    for (size_t i = 0; i < c->bufs.size(); ++i) {
        if (c->bufs[i].state != BUF_FREE) {
            n++;
        }
    }
    return n;
}

// 函数: demo1_capture_thread
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo1_capture_thread(void* arg) {
    Demo1Ctx* c = (Demo1Ctx*)arg;
    for (int f = 0; f < c->total_frames; ++f) {
        // 步骤1: 等待一个 FREE 缓冲
        pthread_mutex_lock(&c->mtx);
        int idx = -1;
        int64_t wait_begin = 0;
        while ((idx = find_state(c, BUF_FREE)) < 0) {
            if (wait_begin == 0) {
                wait_begin = now_us();
            }
            pthread_cond_wait(&c->cond, &c->mtx);
        }
        if (wait_begin > 0) {
            c->cap_wait_us += (now_us() - wait_begin);
        }

        // 占用空闲缓冲，填入帧号
        c->bufs[idx].state = BUF_CAPTURED;
        c->bufs[idx].frame_id = f;
        pthread_mutex_unlock(&c->mtx);

        // 步骤2: 在锁外做耗时工作（避免长时间持锁）
        usleep(c->capture_ms * 1000);

        // 步骤3: 转入编码等待状态
        pthread_mutex_lock(&c->mtx);
        c->bufs[idx].state = BUF_QUEUED_ENC;
        c->produced++;
        pthread_cond_broadcast(&c->cond);
        pthread_mutex_unlock(&c->mtx);
    }

    // 全部帧产出后，发布 cap_done 信号让下游可判断退出条件
    pthread_mutex_lock(&c->mtx);
    c->cap_done = 1;
    pthread_cond_broadcast(&c->cond);
    pthread_mutex_unlock(&c->mtx);
    return NULL;
}

// 函数: demo1_encode_thread
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo1_encode_thread(void* arg) {
    Demo1Ctx* c = (Demo1Ctx*)arg;
    while (1) {
        // 步骤1: 等待编码输入（QUEUED_ENC）
        pthread_mutex_lock(&c->mtx);
        int idx = -1;
        int64_t wait_begin = 0;
        while ((idx = find_state(c, BUF_QUEUED_ENC)) < 0) {
            // 若上游已完成且没有可编码帧，编码线程可退出
            if (c->cap_done) {
                pthread_mutex_unlock(&c->mtx);
                return NULL;
            }
            if (wait_begin == 0) {
                wait_begin = now_us();
            }
            pthread_cond_wait(&c->cond, &c->mtx);
        }
        if (wait_begin > 0) {
            c->enc_wait_us += (now_us() - wait_begin);
        }
        c->bufs[idx].state = BUF_ENCODING;
        pthread_mutex_unlock(&c->mtx);

        // 步骤2: 模拟编码
        usleep(c->encode_ms * 1000);

        // 步骤3: 产出到显示队列
        pthread_mutex_lock(&c->mtx);
        c->bufs[idx].state = BUF_QUEUED_OUT;
        c->encoded++;
        pthread_cond_broadcast(&c->cond);
        pthread_mutex_unlock(&c->mtx);
    }
}

// 函数: demo1_display_thread
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo1_display_thread(void* arg) {
    Demo1Ctx* c = (Demo1Ctx*)arg;
    while (1) {
        // 步骤1: 等待可显示缓冲（QUEUED_OUT）
        pthread_mutex_lock(&c->mtx);
        int idx = -1;
        int64_t wait_begin = 0;
        while ((idx = find_state(c, BUF_QUEUED_OUT)) < 0) {
            // 退出条件:
            // - capture 已结束
            // - 所有帧都已编码完成
            // - 当前没有可显示缓冲
            if (c->cap_done && c->encoded >= c->total_frames) {
                pthread_mutex_unlock(&c->mtx);
                return NULL;
            }
            if (wait_begin == 0) {
                wait_begin = now_us();
            }
            pthread_cond_wait(&c->cond, &c->mtx);
        }
        if (wait_begin > 0) {
            c->dsp_wait_us += (now_us() - wait_begin);
        }
        c->bufs[idx].state = BUF_DISPLAYING;
        pthread_mutex_unlock(&c->mtx);

        // 步骤2: 模拟显示
        usleep(c->display_ms * 1000);

        pthread_mutex_lock(&c->mtx);
        // 显示完后归还到 FREE，形成“池内复用闭环”
        c->bufs[idx].state = BUF_FREE;
        c->bufs[idx].frame_id = -1;
        c->displayed++;
        pthread_cond_broadcast(&c->cond);
        pthread_mutex_unlock(&c->mtx);
    }
}

// 函数: run_demo1
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo1(int argc, char** argv) {
    int frames = arg_or_default(argc, argv, 2, 120);
    int capture_ms = arg_or_default(argc, argv, 3, 8);
    int encode_ms = arg_or_default(argc, argv, 4, 12);
    int display_ms = arg_or_default(argc, argv, 5, 6);
    int pool = arg_or_default(argc, argv, 6, 4);

    // 参数兜底，避免非法输入破坏实验
    if (frames <= 0) frames = 1;
    if (capture_ms <= 0) capture_ms = 1;
    if (encode_ms <= 0) encode_ms = 1;
    if (display_ms <= 0) display_ms = 1;
    if (pool < 2) pool = 2;

    Demo1Ctx c;
    memset(&c, 0, sizeof(c));
    c.total_frames = frames;
    c.capture_ms = capture_ms;
    c.encode_ms = encode_ms;
    c.display_ms = display_ms;
    c.bufs.resize((size_t)pool);
    // 初始化固定缓冲池：所有缓冲起始都在 FREE
    for (int i = 0; i < pool; ++i) {
        c.bufs[(size_t)i].id = i;
        c.bufs[(size_t)i].frame_id = -1;
        c.bufs[(size_t)i].state = BUF_FREE;
    }
    pthread_mutex_init(&c.mtx, NULL);
    pthread_cond_init(&c.cond, NULL);

    // 启动三段流水线
    pthread_t tcap, tenc, tdsp;
    int64_t begin = now_us();
    pthread_create(&tcap, NULL, demo1_capture_thread, &c);
    pthread_create(&tenc, NULL, demo1_encode_thread, &c);
    pthread_create(&tdsp, NULL, demo1_display_thread, &c);
    pthread_join(tcap, NULL);
    pthread_join(tenc, NULL);
    pthread_join(tdsp, NULL);
    int64_t end = now_us();

    // 收尾一致性检查:
    // 所有缓冲应回到 FREE，若不为 0 说明生命周期没闭环
    int leaks = count_not_free(&c);
    printf("[demo1 buffer_lifecycle] frames=%d pool=%d cap=%dms enc=%dms dsp=%dms\n",
           frames, pool, capture_ms, encode_ms, display_ms);
    printf("produced=%d encoded=%d displayed=%d elapsed=%.2fms\n",
           c.produced, c.encoded, c.displayed, (end - begin) / 1000.0);
    printf("wait_ms: cap=%.2f enc=%.2f dsp=%.2f\n",
           c.cap_wait_us / 1000.0, c.enc_wait_us / 1000.0, c.dsp_wait_us / 1000.0);
    printf("buffer_leaks=%d (期望 0)\n", leaks);
    printf("解读: cap_wait 高通常是下游慢导致上游拿不到 free buffer。\n");

    pthread_cond_destroy(&c.cond);
    pthread_mutex_destroy(&c.mtx);
    return 0;
}

// ==========================================================
// Demo 2: 实时链路队列策略（阻塞 vs 丢旧帧）
// ==========================================================
//
// 这个 demo 是“低延迟系统设计”的核心实验。
// 同样的输入条件下，对比两种队列溢出策略:
//
// POLICY_BLOCK:
// - 满了就等，不丢帧
// - 代价: 排队会越积越多，延迟可能持续抬升
//
// POLICY_DROP_OLDEST:
// - 满了丢最旧帧，把更新的帧放进去
// - 代价: 丢帧，但常能明显降低延迟
//
// 这对应真实取舍:
// - 直播预览/视频会议: 更看重实时性（常倾向丢帧）
// - 录像归档/离线处理: 更看重完整性（常倾向阻塞）

enum OverflowPolicy {
    POLICY_BLOCK = 0,
    POLICY_DROP_OLDEST = 1,
};

struct LiveFrame {
    int id;
    int64_t cap_ms;
};

struct LiveQueueStats {
    int dropped;          // 被主动丢弃的帧数（仅 DROP_OLDEST 会增加）
    int64_t push_wait_us; // 生产者因为队列满等待的总时长（BLOCK 关注）
    int peak_depth;       // 队列最大深度
};

class LiveQueue {
   public:
    LiveQueue(int cap, OverflowPolicy p) : cap_(cap), policy_(p), closed_(false) {
        pthread_mutex_init(&mtx_, NULL);
        pthread_cond_init(&not_full_, NULL);
        pthread_cond_init(&not_empty_, NULL);
        stats_.dropped = 0;
        stats_.push_wait_us = 0;
        stats_.peak_depth = 0;
    }

    ~LiveQueue() {
        pthread_mutex_destroy(&mtx_);
        pthread_cond_destroy(&not_full_);
        pthread_cond_destroy(&not_empty_);
    }

    // 函数: push
    // 参数: const LiveFrame& f
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    bool push(const LiveFrame& f) {
        pthread_mutex_lock(&mtx_);
        int64_t wait_begin = 0;

        // 当队列满时，策略分叉:
        // - BLOCK: 等消费者腾出空间
        // - DROP_OLDEST: 丢最旧数据后写入新数据
        while (!closed_ && (int)q_.size() >= cap_) {
            if (policy_ == POLICY_DROP_OLDEST) {
                // 低延迟策略:
                // 队列满时先丢最旧帧，把“更近实时”的新帧塞进去。
                q_.pop_front();
                stats_.dropped++;
                break;
            } else {
                // 保完整策略:
                // 队列满就等下游消费，保证不丢帧但可能让延迟积累。
                if (wait_begin == 0) {
                    wait_begin = now_us();
                }
                pthread_cond_wait(&not_full_, &mtx_);
            }
        }
        if (wait_begin > 0) {
            stats_.push_wait_us += (now_us() - wait_begin);
        }
        if (closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }
        q_.push_back(f);
        stats_.peak_depth = std::max(stats_.peak_depth, (int)q_.size());
        pthread_cond_signal(&not_empty_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    // 函数: pop
    // 参数: LiveFrame* out
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    bool pop(LiveFrame* out) {
        pthread_mutex_lock(&mtx_);
        // 空队列时等待新数据到来
        while (q_.empty() && !closed_) {
            pthread_cond_wait(&not_empty_, &mtx_);
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

    // 函数: close
    // 参数: 
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    void close() {
        pthread_mutex_lock(&mtx_);
        closed_ = true;
        pthread_cond_broadcast(&not_full_);
        pthread_cond_broadcast(&not_empty_);
        pthread_mutex_unlock(&mtx_);
    }

    // 函数: stats
    // 参数: 
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    LiveQueueStats stats() {
        pthread_mutex_lock(&mtx_);
        LiveQueueStats s = stats_;
        pthread_mutex_unlock(&mtx_);
        return s;
    }

   private:
    int cap_;
    OverflowPolicy policy_;
    bool closed_;
    std::deque<LiveFrame> q_;
    LiveQueueStats stats_;
    pthread_mutex_t mtx_;
    pthread_cond_t not_full_;
    pthread_cond_t not_empty_;
};

struct Demo2Ctx {
    int total_frames;
    int cap_interval_ms;
    int enc_ms;
    LiveQueue* q;

    volatile int done; // 保留标记（本 demo 中主要通过 queue close 结束）
    int produced;
    int consumed;
    int64_t total_queue_delay_ms;
    int64_t max_queue_delay_ms;
};

// 函数: demo2_capture
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo2_capture(void* arg) {
    Demo2Ctx* c = (Demo2Ctx*)arg;
    for (int i = 0; i < c->total_frames; ++i) {
        LiveFrame f;
        f.id = i;
        f.cap_ms = now_ms();
        // 队列关闭时 push 返回 false，线程退出
        if (!c->q->push(f)) {
            break;
        }
        c->produced++;
        usleep(c->cap_interval_ms * 1000);
    }
    c->q->close();
    c->done = 1;
    return NULL;
}

// 函数: demo2_encode
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo2_encode(void* arg) {
    Demo2Ctx* c = (Demo2Ctx*)arg;
    LiveFrame f;
    while (c->q->pop(&f)) {
        // 关键指标: 帧在队列里等待了多久
        int64_t q_delay = now_ms() - f.cap_ms;
        c->consumed++;
        c->total_queue_delay_ms += q_delay;
        c->max_queue_delay_ms = std::max(c->max_queue_delay_ms, q_delay);
        usleep(c->enc_ms * 1000);
    }
    return NULL;
}

// 函数: run_demo2_case
// 参数: OverflowPolicy p, int frames, int cap_interval_ms, int enc_ms, int cap
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void run_demo2_case(OverflowPolicy p, int frames, int cap_interval_ms, int enc_ms, int cap) {
    LiveQueue q(cap, p);
    Demo2Ctx c;
    memset(&c, 0, sizeof(c));
    c.total_frames = frames;
    c.cap_interval_ms = cap_interval_ms;
    c.enc_ms = enc_ms;
    c.q = &q;

    pthread_t tcap, tenc;
    int64_t begin = now_us();
    pthread_create(&tcap, NULL, demo2_capture, &c);
    pthread_create(&tenc, NULL, demo2_encode, &c);
    pthread_join(tcap, NULL);
    pthread_join(tenc, NULL);
    int64_t end = now_us();

    // 统计快照
    LiveQueueStats s = q.stats();
    double avg_q_delay = (c.consumed > 0) ? (double)c.total_queue_delay_ms / c.consumed : 0.0;
    const char* name = (p == POLICY_BLOCK) ? "BLOCK" : "DROP_OLDEST";

    printf("[demo2 live_policy][%s] frames=%d queue_cap=%d cap_interval=%dms enc=%dms\n",
           name, frames, cap, cap_interval_ms, enc_ms);
    printf("produced=%d consumed=%d dropped=%d elapsed=%.2fms\n",
           c.produced, c.consumed, s.dropped, (end - begin) / 1000.0);
    printf("queue_delay_ms: avg=%.2f max=%lld push_wait_ms=%.2f peak_depth=%d\n",
           avg_q_delay, (long long)c.max_queue_delay_ms, s.push_wait_us / 1000.0, s.peak_depth);
}

// 函数: run_demo2
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo2(int argc, char** argv) {
    int frames = arg_or_default(argc, argv, 2, 180);
    int cap_interval_ms = arg_or_default(argc, argv, 3, 6);
    int enc_ms = arg_or_default(argc, argv, 4, 12);
    int cap = arg_or_default(argc, argv, 5, 8);
    if (frames <= 0) frames = 1;
    if (cap_interval_ms <= 0) cap_interval_ms = 1;
    if (enc_ms <= 0) enc_ms = 1;
    if (cap < 1) cap = 1;

    // 同一组输入参数下连跑两种策略，便于直接横向对比
    run_demo2_case(POLICY_BLOCK, frames, cap_interval_ms, enc_ms, cap);
    run_demo2_case(POLICY_DROP_OLDEST, frames, cap_interval_ms, enc_ms, cap);
    printf("解读: 低延迟实时链路通常更偏向 DROP_OLDEST；录像归档更偏向 BLOCK。\n");
    return 0;
}

// ==========================================================
// Demo 3: 动态码控参数更新（rwlock + 生效边界）
// ==========================================================
//
// 目标:
// 1) 演示“控制线程低频写 + 编码线程高频读”的经典并发模型
// 2) 演示“参数更新频率很高，但不一定立刻生效”
//
// 为什么需要这个 demo:
// - 真实编码器里，码控参数常由控制面动态调整
// - 但参数生效常与关键帧边界、内部状态机相关
// - 所以你看到“控制线程写了”不等于“下一帧立刻变”

struct RcConfig {
    int bitrate_kbps; // 码率
    int gop;          // GOP 长度
    int qp;           // QP
    uint64_t version; // 配置版本号（每次写+1）
};

struct Demo3Ctx {
    int frames;
    int encode_ms;
    int ctrl_period_ms;

    pthread_rwlock_t rw; // 读写锁：读并发，写独占
    RcConfig cfg;        // 当前配置
    volatile int stop;   // 控制线程停止标记

    int frames_encoded; // 已编码帧数
    int cfg_updates;    // 控制线程总更新次数
    int cfg_applied;    // 编码线程真正应用新配置的次数
    int stale_frames;  // 读到了新配置但当前帧不是生效边界（例如非关键帧）
};

// 函数: demo3_control
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo3_control(void* arg) {
    Demo3Ctx* c = (Demo3Ctx*)arg;
    while (!c->stop) {
        // 周期性触发一次控制更新
        usleep(c->ctrl_period_ms * 1000);
        pthread_rwlock_wrlock(&c->rw);
        c->cfg.bitrate_kbps += 300;
        if (c->cfg.bitrate_kbps > 6000) {
            c->cfg.bitrate_kbps = 1500;
        }
        c->cfg.qp = 20 + (int)(c->cfg.version % 8);
        c->cfg.gop = 30;
        c->cfg.version++; // 每次写入都推进版本号
        pthread_rwlock_unlock(&c->rw);
        c->cfg_updates++;
    }
    return NULL;
}

// 函数: demo3_encode
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void* demo3_encode(void* arg) {
    Demo3Ctx* c = (Demo3Ctx*)arg;
    uint64_t applied_ver = 0;
    for (int i = 0; i < c->frames; ++i) {
        RcConfig snap;
        // 高速路径读取配置：用读锁保护一致快照
        pthread_rwlock_rdlock(&c->rw);
        snap = c->cfg;
        pthread_rwlock_unlock(&c->rw);

        // 模拟“关键帧边界生效”:
        // 真实编码器里，很多参数在关键帧边界切换更稳定。
        int is_keyframe = (i % snap.gop == 0) ? 1 : 0;
        if (snap.version != applied_ver) {
            if (is_keyframe) {
                applied_ver = snap.version;
                c->cfg_applied++;
            } else {
                // 已读到新配置但未到生效边界，记为 stale
                c->stale_frames++;
            }
        }

        usleep(c->encode_ms * 1000);
        c->frames_encoded++;
    }
    // 编码结束后通知控制线程停机
    c->stop = 1;
    return NULL;
}

// 函数: run_demo3
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int run_demo3(int argc, char** argv) {
    int frames = arg_or_default(argc, argv, 2, 240);
    int enc_ms = arg_or_default(argc, argv, 3, 8);
    int ctrl_period_ms = arg_or_default(argc, argv, 4, 100);
    if (frames <= 0) frames = 1;
    if (enc_ms <= 0) enc_ms = 1;
    if (ctrl_period_ms <= 0) ctrl_period_ms = 1;

    Demo3Ctx c;
    memset(&c, 0, sizeof(c));
    c.frames = frames;
    c.encode_ms = enc_ms;
    c.ctrl_period_ms = ctrl_period_ms;
    c.cfg.bitrate_kbps = 1500;
    c.cfg.gop = 30;
    c.cfg.qp = 24;
    c.cfg.version = 1;
    pthread_rwlock_init(&c.rw, NULL);

    pthread_t tctrl, tenc;
    pthread_create(&tctrl, NULL, demo3_control, &c);
    pthread_create(&tenc, NULL, demo3_encode, &c);
    pthread_join(tenc, NULL);
    pthread_join(tctrl, NULL);

    printf("[demo3 dynamic_rc] frames=%d enc=%dms ctrl_period=%dms\n",
           frames, enc_ms, ctrl_period_ms);
    printf("frames_encoded=%d cfg_updates=%d cfg_applied=%d stale_frames=%d\n",
           c.frames_encoded, c.cfg_updates, c.cfg_applied, c.stale_frames);
    printf("final_cfg: bitrate=%dkbps gop=%d qp=%d version=%llu\n",
           c.cfg.bitrate_kbps, c.cfg.gop, c.cfg.qp, (unsigned long long)c.cfg.version);
    printf("解读: 配置更新频率过高但生效边界稀疏时，会出现 stale_frames 增多。\n");
    printf("      这意味着“控制面很活跃”不等于“画面参数每帧都变”。\n");

    pthread_rwlock_destroy(&c.rw);
    return 0;
}

// 函数: print_usage
// 参数: const char* bin
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void print_usage(const char* bin) {
    printf("usage:\n");
    printf("  %s buffer_lifecycle [frames] [capture_ms] [encode_ms] [display_ms] [pool]\n", bin);
    printf("  %s live_policy [frames] [capture_interval_ms] [encode_ms] [queue_cap]\n", bin);
    printf("  %s dynamic_rc [frames] [encode_ms] [ctrl_period_ms]\n", bin);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    // 一级分发:
    // 根据第一个子命令进入不同实验场景
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "buffer_lifecycle") == 0) {
        return run_demo1(argc, argv);
    }
    if (strcmp(argv[1], "live_policy") == 0) {
        return run_demo2(argc, argv);
    }
    if (strcmp(argv[1], "dynamic_rc") == 0) {
        return run_demo3(argc, argv);
    }

    print_usage(argv[0]);
    return 1;
}
