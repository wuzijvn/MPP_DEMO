#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <deque>
#include <errno.h>
#include <string.h>

/*
Week2 Day3: 条件变量超时 + 优雅退出协议

你在工作中经常会遇到:
1) 数据流间歇性中断（网络抖动/摄像头暂时无帧）
2) 服务需要停机（用户点停止、上层退出）

如果线程写得不好，常见结果是:
- consumer 永久阻塞（无法退出）
- stop 后还要等很久才退出（体验差）

本实验演示两种停机策略:
- BAD_STOP_ONLY:
    只设置 stop 标记，不主动 close/broadcast 队列
    消费者靠 timedwait 超时“慢慢醒来”退出（退出延迟更高）
- GOOD_STOP_AND_CLOSE:
    stop + close + broadcast，阻塞线程立即被唤醒（推荐）

阅读顺序:
1) 先看 BlockingQueue::pop_timeout 返回值语义
2) 再看 controller_thread 如何触发 stop
3) 最后看 BAD 与 GOOD 两种模式的 stop_to_exit 差异
*/

enum StopMode {
    BAD_STOP_ONLY = 0,
    GOOD_STOP_AND_CLOSE = 1,
};

struct Item {
    int id;
    int64_t produce_ms;
};

static int64_t now_us() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int64_t now_ms() { return now_us() / 1000; }

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
class BlockingQueue {
   public:
    explicit BlockingQueue(size_t cap) : cap_(cap), closed_(false) {
        pthread_mutex_init(&mtx_, NULL);
        pthread_cond_init(&not_full_, NULL);
        pthread_cond_init(&not_empty_, NULL);
    }

    ~BlockingQueue() {
        pthread_mutex_destroy(&mtx_);
        pthread_cond_destroy(&not_full_);
        pthread_cond_destroy(&not_empty_);
    }

    bool push(const T& x) {
        pthread_mutex_lock(&mtx_);
        while (!closed_ && q_.size() >= cap_) {
            pthread_cond_wait(&not_full_, &mtx_);
        }
        if (closed_) {
            pthread_mutex_unlock(&mtx_);
            return false;
        }
        q_.push_back(x);
        pthread_cond_signal(&not_empty_);
        pthread_mutex_unlock(&mtx_);
        return true;
    }

    // 返回值语义:
    // - 1: 成功拿到数据
    // - 0: 超时
    // - -1: 队列关闭且为空（可退出）
    int pop_timeout(T* out, int timeout_ms) {
        pthread_mutex_lock(&mtx_);
        while (q_.empty() && !closed_) {
            timespec ts = timeout_after_ms(timeout_ms);
            int rc = pthread_cond_timedwait(&not_empty_, &mtx_, &ts);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&mtx_);
                return 0;
            }
        }
        if (q_.empty() && closed_) {
            pthread_mutex_unlock(&mtx_);
            return -1;
        }
        *out = q_.front();
        q_.pop_front();
        pthread_cond_signal(&not_full_);
        pthread_mutex_unlock(&mtx_);
        return 1; 
    }

    void close() {
        pthread_mutex_lock(&mtx_);
        closed_ = true;
        pthread_cond_broadcast(&not_full_);
        pthread_cond_broadcast(&not_empty_);
        pthread_mutex_unlock(&mtx_);
    }

   private:
    size_t cap_;
    bool closed_;
    std::deque<T> q_;
    pthread_mutex_t mtx_;
    pthread_cond_t not_full_;
    pthread_cond_t not_empty_;
};

struct Ctx {
    StopMode mode;
    int runtime_ms;      // 实验总时长
    int timeout_ms;      // consumer 每次等待超时
    int process_ms;      // consumer 处理一条数据耗时
    int burst_items;     // producer 每个 burst 产出条目
    int burst_gap_ms;    // burst 间隔

    BlockingQueue<Item>* q;

    volatile int stop_requested; // 控制线程发起停机
    volatile int producer_done;  // 生产线程退出标记
    volatile int consumer_done;  // 消费线程退出标记

    int produced;
    int consumed;
    int timeout_hits;
    int64_t total_latency_ms;
    int64_t stop_at_ms;
    int64_t consumer_exit_ms;
};

static void* producer_thread(void* arg) {
    Ctx* c = (Ctx*)arg;
    int seq = 0;
    while (!c->stop_requested) {
        // burst 生产：模拟“偶尔来一批数据”的输入流
        for (int i = 0; i < c->burst_items; ++i) {
            if (c->stop_requested) {
                break;
            }
            Item x;
            x.id = seq++;
            x.produce_ms = now_ms();
            if (!c->q->push(x)) {
                c->producer_done = 1;
                return NULL;
            }
            c->produced++;
            usleep(8 * 1000);
        }
        usleep(c->burst_gap_ms * 1000);
    }
    c->producer_done = 1;
    return NULL;
}

static void* consumer_thread(void* arg) {
    Ctx* c = (Ctx*)arg;
    while (1) {
        Item x;
        int rc = c->q->pop_timeout(&x, c->timeout_ms);
        if (rc == 1) {
            usleep(c->process_ms * 1000);
            c->consumed++;
            c->total_latency_ms += (now_ms() - x.produce_ms);
            continue;
        }
        if (rc == -1) {
            // 队列关闭且为空，属于“明确结束”
            break;
        }

        // rc == 0 超时:
        c->timeout_hits++;
        if (c->stop_requested && c->producer_done) {
            // BAD_STOP_ONLY 模式下，通常会走到这里退出
            break;
        }
    }
    c->consumer_done = 1;
    c->consumer_exit_ms = now_ms();
    return NULL;
}

static void* controller_thread(void* arg) {
    Ctx* c = (Ctx*)arg;
    usleep(c->runtime_ms * 1000);
    c->stop_requested = 1;
    c->stop_at_ms = now_ms();

    if (c->mode == GOOD_STOP_AND_CLOSE) {
        // 推荐方案: 立刻 close，唤醒所有阻塞线程
        c->q->close();
    }
    return NULL;
}

static const char* mode_name(StopMode m) {
    return (m == GOOD_STOP_AND_CLOSE) ? "GOOD_STOP_AND_CLOSE" : "BAD_STOP_ONLY";
}

static void run_case(StopMode mode, int runtime_ms, int timeout_ms) {
    BlockingQueue<Item> q(16);
    Ctx c;
    memset(&c, 0, sizeof(c));
    c.mode = mode;
    c.runtime_ms = runtime_ms;
    c.timeout_ms = timeout_ms;
    c.process_ms = 12;
    c.burst_items = 5;
    c.burst_gap_ms = 120;
    c.q = &q;

    pthread_t tp, tc, tctrl;
    pthread_create(&tp, NULL, producer_thread, &c);
    pthread_create(&tc, NULL, consumer_thread, &c);
    pthread_create(&tctrl, NULL, controller_thread, &c);

    pthread_join(tctrl, NULL);
    pthread_join(tp, NULL);
    if (mode == BAD_STOP_ONLY) {
        // BAD 模式为了让 consumer 不永远阻塞，最终还是 close 一次。
        // 但注意: 这是“控制线程 stop 很久以后”才做，退出通常更慢。
        q.close();
    }
    pthread_join(tc, NULL);

    double avg_latency = (c.consumed > 0) ? (double)c.total_latency_ms / c.consumed : 0.0;
    double exit_delay = (c.consumer_exit_ms > 0 && c.stop_at_ms > 0)
                            ? (double)(c.consumer_exit_ms - c.stop_at_ms)
                            : 0.0;

    printf("[%s] produced=%d consumed=%d timeout_hits=%d avg_latency=%.2fms stop_to_exit=%.2fms\n",
           mode_name(mode), c.produced, c.consumed, c.timeout_hits, avg_latency, exit_delay);
}

int main(int argc, char** argv) {
    // 参数:
    // [1] runtime_ms(default 3000)
    // [2] timeout_ms(default 80)
    int runtime_ms = (argc > 1) ? atoi(argv[1]) : 3000;
    int timeout_ms = (argc > 2) ? atoi(argv[2]) : 80;
    if (runtime_ms <= 0) runtime_ms = 1000;
    if (timeout_ms <= 0) timeout_ms = 10;

    printf("config: runtime=%dms timeout=%dms\n", runtime_ms, timeout_ms);
    run_case(BAD_STOP_ONLY, runtime_ms, timeout_ms);
    run_case(GOOD_STOP_AND_CLOSE, runtime_ms, timeout_ms);

    printf("\n解读:\n");
    printf("1) GOOD_STOP_AND_CLOSE 的 stop_to_exit 通常更小，退出更干脆。\n");
    printf("2) timeout_hits 高不一定是 bug，可能只是输入间歇性断流。\n");
    printf("3) 生产代码中，停机优先采用: stop_flag + queue.close + join。\n");
    return 0;
}
