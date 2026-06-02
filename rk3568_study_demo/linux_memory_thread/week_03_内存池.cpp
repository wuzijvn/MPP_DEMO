#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <deque>
#include <vector>

/*
Week3 主题: 固定内存池（Fixed Buffer Pool）

这个实验在音视频场景里的意义:
1) 视频帧很大（例如 1080p YUV420 一帧约 3MB），频繁 malloc/free 成本高且易碎片化
2) 工程中常用“预分配固定数量缓冲”+“循环复用”
3) 当池为空时，生产者应等待，而不是继续向系统申请新内存

你要观察:
1) system_alloc_count 是否等于 pool_count（说明没有运行时额外分配）
2) 多线程并发 acquire/release 是否稳定
*/

/*
函数与参数速查:
1) FixedBufferPool(buffer_count, buffer_size):
   - buffer_count: 池中缓冲总数（并发容量）。
   - buffer_size: 单缓冲大小（通常按帧大小配置）。
2) acquire():
   - 无空闲缓冲时阻塞等待，返回可写 Buffer*。
3) release(Buffer* b):
   - b 是归还对象指针，只回收到空闲队列，不释放底层内存。
4) system_alloc_count():
   - 返回初始化阶段向系统申请内存的次数，理想等于 buffer_count。
5) pool_worker(arg):
   - arg 实际是 WorkerCtx*，循环执行“申请-写入-归还”。
*/

struct Buffer {
    int id;          // 缓冲编号（池内唯一）
    uint8_t* data;   // 实际内存地址
    size_t size;     // 缓冲字节数
};

class FixedBufferPool {
   public:
    // 初始化时一次性申请全部缓冲，后续运行阶段不再 malloc。
    FixedBufferPool(size_t buffer_count, size_t buffer_size)
        : buffer_size_(buffer_size), system_alloc_count_(0) {
        pthread_mutex_init(&mtx_, NULL);
        pthread_cond_init(&not_empty_, NULL);
        buffers_.resize(buffer_count);
        for (size_t i = 0; i < buffer_count; ++i) {
            buffers_[i].id = (int)i;
            buffers_[i].size = buffer_size_;
            buffers_[i].data = (uint8_t*)malloc(buffer_size_);
            if (!buffers_[i].data) {
                fprintf(stderr, "malloc failed for buffer %zu\n", i);
                exit(1);
            }
            system_alloc_count_++; // 统计系统层面的分配次数
            free_ids_.push_back((int)i);
        }
    }

    ~FixedBufferPool() {
        for (size_t i = 0; i < buffers_.size(); ++i) {
            free(buffers_[i].data);
        }
        pthread_cond_destroy(&not_empty_);
        pthread_mutex_destroy(&mtx_);
    }

    // 函数: acquire
    // 参数: 
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    Buffer* acquire() {
        pthread_mutex_lock(&mtx_);
        // 池空时等待，有缓冲归还后再继续。
        while (free_ids_.empty()) {
            pthread_cond_wait(&not_empty_, &mtx_);
        }
        int id = free_ids_.front();
        free_ids_.pop_front();
        pthread_mutex_unlock(&mtx_);
        return &buffers_[id];
    }

    // 函数: release
    // 参数: Buffer* b
    // 说明: 见本函数内部逻辑与文件顶部实验说明。
    void release(Buffer* b) {
        pthread_mutex_lock(&mtx_);
        // 归还时仅把 id 放回空闲队列，不做 free。
        free_ids_.push_back(b->id);
        pthread_cond_signal(&not_empty_);
        pthread_mutex_unlock(&mtx_);
    }

    size_t system_alloc_count() const { return system_alloc_count_; }

   private:
    size_t buffer_size_;
    size_t system_alloc_count_;
    std::vector<Buffer> buffers_;
    std::deque<int> free_ids_;
    pthread_mutex_t mtx_;
    pthread_cond_t not_empty_;
};

struct WorkerCtx {
    FixedBufferPool* pool; // 共享内存池
    int worker_id;         // 线程编号
    int loops;             // 循环次数
};

// 函数: pool_worker
// 参数: void* arg
// 说明: 见本函数内部逻辑与文件顶部实验说明。
void* pool_worker(void* arg) {
    WorkerCtx* ctx = (WorkerCtx*)arg;
    uint64_t checksum = 0;
    for (int i = 0; i < ctx->loops; ++i) {
        // 1) 从池里拿缓冲
        Buffer* b = ctx->pool->acquire();
        // 2) 模拟写入（这里仅写前 64 字节作为示例）
        memset(b->data, (ctx->worker_id + i) & 0xFF, 64);
        checksum += b->data[0];
        // 3) 归还
        ctx->pool->release(b);
    }
    printf("worker=%d done, checksum=%llu\n", ctx->worker_id,
           (unsigned long long)checksum);
    return NULL;
}

// 函数: main
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main() {
    // 模拟 1080p YUV420 单帧大小
    const size_t pool_count = 8;
    const size_t yuv420_size = 1920 * 1080 * 3 / 2;
    const int worker_num = 3;
    const int loops = 8000;

    FixedBufferPool pool(pool_count, yuv420_size);

    pthread_t tids[worker_num];
    WorkerCtx ctxs[worker_num];
    for (int i = 0; i < worker_num; ++i) {
        ctxs[i].pool = &pool;
        ctxs[i].worker_id = i;
        ctxs[i].loops = loops;
        pthread_create(&tids[i], NULL, pool_worker, &ctxs[i]);
    }
    for (int i = 0; i < worker_num; ++i) {
        pthread_join(tids[i], NULL);
    }

    printf("pool buffers=%zu, system_alloc_count=%zu, buffer_size=%zu bytes\n",
           pool_count, pool.system_alloc_count(), yuv420_size);
    printf("解读: system_alloc_count 理论上应等于 pool_count，表示运行时没有额外分配。\n");
    return 0;
}
