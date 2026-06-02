#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <deque>
#include <vector>

// ============================================
// Week2 Day2: 队列容量与背压实验
// 目标:
// 1) 观察队列容量对吞吐/时延的影响
// 2) 用等待时间定位瓶颈到底在上游还是下游
// 3) 把“背压”从概念变成可量化指标
//
// 你将得到的核心能力:
// - 看见“卡顿”时，不再凭感觉猜，而是用指标判断:
//   * 谁在等谁（push_wait / pop_wait）
//   * 积压发生在哪段（raw 队列还是 pkt 队列）
//   * 扩大队列容量究竟提升了吞吐，还是只是把延迟藏起来
//
// 指标速查:
// - raw_push_wait_us 高:
//     CAP -> ENC 队列常满，上游 CAP 被 ENC 背压
// - raw_pop_wait_us 高:
//     ENC 经常拿不到原始帧，说明 CAP 偏慢
// - pkt_push_wait_us 高:
//     ENC -> REN 队列常满，中游 ENC 被 REN 背压
// - pkt_pop_wait_us 高:
//     REN 经常拿不到包，说明 ENC 偏慢
//
// 一个常见误区:
// - 队列容量从 1 增到 8，吞吐未必上升，但平均延迟常上升。
// - 原因: 更多帧在队列里排队，等待时间变长（排队论中的排队时延）。
//
// 阅读顺序:
// 1) 先看 BoundedQueue::push / pop 里的等待逻辑
// 2) 再看三线程 capture/encode/render
// 3) 最后看输出表格 print_one_row 如何把指标串起来
// ============================================

static int64_t now_us() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int64_t now_ms() { return now_us() / 1000; }

struct QueueStats {
    uint64_t push_wait_count;  // 生产者因队列满而进入等待的次数
    uint64_t pop_wait_count;   // 消费者因队列空而进入等待的次数
    int64_t push_wait_us;      // 生产者累计等待时长
    int64_t pop_wait_us;       // 消费者累计等待时长
    uint64_t peak_depth;       // 队列峰值深度
};

template <typename T>
class BoundedQueue {
   public:
    // cap 表示“队列最大在途元素数”。
    // cap 越小，背压越早发生；cap 越大，吸收抖动能力越强但排队时延可能升高。
    explicit BoundedQueue(size_t cap) : cap_(cap), closed_(false) {
        pthread_mutex_init(&mtx_, NULL);
        pthread_cond_init(&not_full_, NULL);
        pthread_cond_init(&not_empty_, NULL);
        stats_.push_wait_count = 0;
        stats_.pop_wait_count = 0;
        stats_.push_wait_us = 0;
        stats_.pop_wait_us = 0;
        stats_.peak_depth = 0;
    }

    ~BoundedQueue() {
        pthread_mutex_destroy(&mtx_);
        pthread_cond_destroy(&not_full_);
        pthread_cond_destroy(&not_empty_);
    }

    bool push(const T& item) {
        pthread_mutex_lock(&mtx_);
        int64_t wait_begin = 0;

        // 背压点 1:
        // 队列满了，上游(生产者)只能等待下游消费。
        while (!closed_ && q_.size() >= cap_) {
            if (wait_begin == 0) {
                wait_begin = now_us();
            }
            stats_.push_wait_count++;
            pthread_cond_wait(&not_full_, &mtx_);
        }
        if (wait_begin != 0) {
            stats_.push_wait_us += (now_us() - wait_begin);
        }

        // 关闭后不再接收新数据。
        if (closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }

        q_.push_back(item);
        if (q_.size() > stats_.peak_depth) {
            stats_.peak_depth = q_.size();
        }

        // 有新数据后，唤醒一个消费者。
        pthread_cond_signal(&not_empty_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    bool pop(T* out) {
        pthread_mutex_lock(&mtx_);
        int64_t wait_begin = 0;

        // 背压点 2:
        // 队列空了，下游(消费者)只能等待上游生产。
        while (q_.empty() && !closed_) {
            if (wait_begin == 0) {
                wait_begin = now_us();
            }
            stats_.pop_wait_count++;
            pthread_cond_wait(&not_empty_, &mtx_);
        }
        if (wait_begin != 0) {
            stats_.pop_wait_us += (now_us() - wait_begin);
        }

        // close 不代表丢数据。
        // 只有“队列空 + 已关闭”才表示真正结束。
        if (q_.empty() && closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }

        *out = q_.front();
        q_.pop_front();
        // 消费后释放一个槽位，唤醒可能阻塞的生产者。
        pthread_cond_signal(&not_full_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    void close() {
        pthread_mutex_lock(&mtx_);
        closed_ = true;
        pthread_cond_broadcast(&not_full_);
        pthread_cond_broadcast(&not_empty_);
        pthread_mutex_unlock(&mtx_);
    }

    // 在锁保护下抓取统计快照，保证读到的是一致视图。
    QueueStats snapshot() {
        pthread_mutex_lock(&mtx_);
        QueueStats out = stats_;
        pthread_mutex_unlock(&mtx_);
        return out;
    }

   private:
    size_t cap_;
    bool closed_;
    std::deque<T> q_;
    QueueStats stats_;
    pthread_mutex_t mtx_;
    pthread_cond_t not_full_;
    pthread_cond_t not_empty_;
};

struct Frame {
    int id;
    int64_t capture_ms;
};

struct Packet {
    int id;
    int64_t capture_ms;
    int64_t encode_done_ms;
};

struct PipelineConfig {
    int frames;
    int capture_ms;  // CAP线程每帧耗时
    int encode_ms;   // ENC线程每帧耗时
    int render_ms;   // REN线程每帧耗时
    int raw_cap;     // CAP->ENC 队列容量
    int pkt_cap;     // ENC->REN 队列容量
};

struct PipelineResult {
    double elapsed_ms;
    double throughput_fps;
    double avg_e2e_ms;
    double max_e2e_ms;
    // 两段队列各自的背压统计:
    // raw_stats 关注 CAP<->ENC
    // pkt_stats 关注 ENC<->REN
    QueueStats raw_stats;
    QueueStats pkt_stats;
};

struct PipelineCtx {
    PipelineConfig cfg;
    BoundedQueue<Frame>* raw_q;
    BoundedQueue<Packet>* pkt_q;

    int consumed;
    int64_t total_e2e_ms;
    int64_t max_e2e_ms;
};

static void* capture_thread(void* arg) {
    PipelineCtx* ctx = (PipelineCtx*)arg;
    for (int i = 0; i < ctx->cfg.frames; ++i) {
        Frame f;
        f.id = i;
        f.capture_ms = now_ms();
        // push 可能因 close 失败（下游已终止）。
        if (!ctx->raw_q->push(f)) {
            break;
        }
        usleep(ctx->cfg.capture_ms * 1000);
    }
    // CAP 完成后关闭 raw_q，通知 ENC 不会再有新帧。
    ctx->raw_q->close();
    return NULL;
}
 
static void* encode_thread(void* arg) {
    PipelineCtx* ctx = (PipelineCtx*)arg;
    Frame f;
    while (ctx->raw_q->pop(&f)) {
        usleep(ctx->cfg.encode_ms * 1000);
        Packet p;
        p.id = f.id;
        p.capture_ms = f.capture_ms;
        p.encode_done_ms = now_ms();
        if (!ctx->pkt_q->push(p)) {
            break;
        }
    }
    // ENC 完成后关闭 pkt_q，通知 REN 可收尾退出。
    ctx->pkt_q->close();
    return NULL;
}

static void* render_thread(void* arg) {
    PipelineCtx* ctx = (PipelineCtx*)arg;
    Packet p;
    while (ctx->pkt_q->pop(&p)) {
        usleep(ctx->cfg.render_ms * 1000);
        // 端到端时延=当前时刻-初始采集时刻
        // 不用 encode_done_ms 作为基准，是因为我们关心全链路体验时延。
        int64_t e2e = now_ms() - p.capture_ms;
        ctx->consumed++;
        ctx->total_e2e_ms += e2e;
        if (e2e > ctx->max_e2e_ms) {
            ctx->max_e2e_ms = e2e;
        }
    }
    return NULL;
}

static PipelineResult run_once(const PipelineConfig& cfg) {
    BoundedQueue<Frame> raw_q((size_t)cfg.raw_cap);
    BoundedQueue<Packet> pkt_q((size_t)cfg.pkt_cap);

    PipelineCtx ctx;
    ctx.cfg = cfg;
    ctx.raw_q = &raw_q;
    ctx.pkt_q = &pkt_q;
    ctx.consumed = 0;
    ctx.total_e2e_ms = 0;
    ctx.max_e2e_ms = 0;

    // 单次实验完整时长: 从三线程启动到三线程全部结束。
    pthread_t t_cap, t_enc, t_ren;
    int64_t begin = now_us();
    pthread_create(&t_cap, NULL, capture_thread, &ctx);
    pthread_create(&t_enc, NULL, encode_thread, &ctx);
    pthread_create(&t_ren, NULL, render_thread, &ctx);
    pthread_join(t_cap, NULL);
    pthread_join(t_enc, NULL);
    pthread_join(t_ren, NULL);
    int64_t end = now_us();

    PipelineResult r;
    r.elapsed_ms = (end - begin) / 1000.0;
    r.throughput_fps = (cfg.frames * 1000.0) / r.elapsed_ms;
    r.avg_e2e_ms = (ctx.consumed > 0) ? (double)ctx.total_e2e_ms / ctx.consumed : 0.0;
    r.max_e2e_ms = (double)ctx.max_e2e_ms;
    r.raw_stats = raw_q.snapshot();
    r.pkt_stats = pkt_q.snapshot();
    return r;
}

static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) {
        return dft;
    }
    return atoi(argv[idx]);
}

static void print_header() {
    printf("raw_cap pkt_cap | elapsed(ms) fps   avg_e2e max_e2e | raw_push_wait raw_pop_wait raw_peak | pkt_push_wait pkt_pop_wait pkt_peak\n");
    printf("----------------+--------------------------------------+-------------------------------------+-------------------------------------\n");
}

static void print_one_row(const PipelineConfig& cfg, const PipelineResult& r) {
    printf("%7d %7d | %10.2f %5.1f %7.2f %7.2f | %13.2f %12.2f %8llu | %13.2f %12.2f %8llu\n",
           cfg.raw_cap, cfg.pkt_cap, r.elapsed_ms, r.throughput_fps, r.avg_e2e_ms, r.max_e2e_ms,
           r.raw_stats.push_wait_us / 1000.0, r.raw_stats.pop_wait_us / 1000.0,
           (unsigned long long)r.raw_stats.peak_depth, r.pkt_stats.push_wait_us / 1000.0,
           r.pkt_stats.pop_wait_us / 1000.0, (unsigned long long)r.pkt_stats.peak_depth);
}

int main(int argc, char** argv) {
    // 单次模式:
    // ./week2_day2 [frames] [capture_ms] [encode_ms] [render_ms] [raw_cap] [pkt_cap]
    //
    // 扫描模式:
    // ./week2_day2 [frames] [capture_ms] [encode_ms] [render_ms] 0 0
    // raw_cap/pkt_cap = 0 时自动扫描 {1,2,4,8}
    PipelineConfig cfg;
    cfg.frames = arg_or_default(argc, argv, 1, 120);
    cfg.capture_ms = arg_or_default(argc, argv, 2, 8);
    cfg.encode_ms = arg_or_default(argc, argv, 3, 14);
    cfg.render_ms = arg_or_default(argc, argv, 4, 6);
    cfg.raw_cap = arg_or_default(argc, argv, 5, 0);
    cfg.pkt_cap = arg_or_default(argc, argv, 6, 0);

    if (cfg.frames <= 0) cfg.frames = 1;
    if (cfg.capture_ms <= 0) cfg.capture_ms = 1;
    if (cfg.encode_ms <= 0) cfg.encode_ms = 1;
    if (cfg.render_ms <= 0) cfg.render_ms = 1;

    printf("config: frames=%d cap=%dms enc=%dms ren=%dms\n",
           cfg.frames, cfg.capture_ms, cfg.encode_ms, cfg.render_ms);
    print_header();

    if (cfg.raw_cap > 0 && cfg.pkt_cap > 0) {
        PipelineResult r = run_once(cfg);
        print_one_row(cfg, r);
        return 0;
    }

    // 自动扫描容量组合，帮助你快速找“拐点”:
    // 常见拐点现象:
    // - 1->2 吞吐明显改善
    // - 4->8 吞吐变化很小，但 avg_e2e 增加明显
    // 这通常意味着“继续加队列只是在堆积延迟”。
    std::vector<int> caps;
    caps.push_back(1);
    caps.push_back(2);
    caps.push_back(4);
    caps.push_back(8);

    for (size_t i = 0; i < caps.size(); ++i) {
        PipelineConfig c = cfg;
        c.raw_cap = caps[i];
        c.pkt_cap = caps[i];
        PipelineResult r = run_once(c);
        print_one_row(c, r);
    }

    printf("\n解读建议:\n");
    printf("1) raw_push_wait 高: CAP->ENC 队列经常满, 上游被背压(常见于 encode 更慢)\n");
    printf("2) raw_pop_wait 高: ENC 经常等不到帧, 常见于 capture 太慢\n");
    printf("3) pkt_push_wait 高: ENC->REN 队列经常满, 常见于 render 更慢\n");
    printf("4) pkt_pop_wait 高: REN 经常等不到包, 常见于 encode 更慢\n");
    printf("5) peak_depth 接近 cap 且等待高, 说明容量已经触顶\n");
    return 0;
}
