#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

/*
Week3 Day3: 实时内存路径与抖动（预分配 + 预热 + mlock）

为什么这对 SoC 音视频关键：
1) 预览/录制/推流是实时系统，最怕“偶发尖峰延迟”
2) 抖动来源常见有：
   - 首次触页 page fault
   - 运行中频繁 malloc/free
   - 内存被换页（swap）或回收导致的访问延迟

这个 demo 对比两种模式：
1) ON_DEMAND:
   每帧 malloc/free + 首次触页发生在实时循环内
2) PREALLOC_PREFAULT:
   启动时预分配 ring buffer + 预热触页 + 尝试 mlock

我们关注：
1) 每帧循环耗时的 avg/p99/max
2) 超过目标周期(deadline)的 miss 数量
*/

/*
函数与参数速查:
1) run_on_demand(frames, frame_bytes, deadline_us):
   - frames: 总帧数。
   - frame_bytes: 单帧缓冲大小。
   - deadline_us: 单帧预算（如 16666us 对应 60fps）。
   - 特征: 每帧 malloc/free，实时环路内触页。
2) run_prealloc_prefault(frames, frame_bytes, ring_count, deadline_us):
   - ring_count: 预分配环形缓冲数量。
   - 特征: 初始化阶段预热触页并尝试 mlock，降低长尾。
3) touch_all_pages(p, bytes):
   - 强制每页触碰，模拟真实大帧写入和触页行为。
4) calc_stats(us, deadline_us):
   - us: 每帧耗时样本（微秒）。
   - 返回 avg/p99/max 以及 deadline miss 数。
*/

// 函数: mono_ns
// 参数: 
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int64_t mono_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// 函数: arg_or_default
// 参数: int argc, char** argv, int idx, int dft
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static int arg_or_default(int argc, char** argv, int idx, int dft) {
    if (idx >= argc) return dft;
    return atoi(argv[idx]);
}

enum Mode {
    MODE_ON_DEMAND = 0,
    MODE_PREALLOC_PREFAULT = 1,
};

struct Stats {
    double avg_us;
    double p99_us;
    double max_us;
    int deadline_miss;
};

// 函数: touch_all_pages
// 参数: uint8_t* p, size_t bytes
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static void touch_all_pages(uint8_t* p, size_t bytes) {
    const size_t kPage = 4096;
    for (size_t off = 0; off < bytes; off += kPage) {
        p[off] = (uint8_t)(off & 0xFF);
    }
    p[bytes - 1] ^= 1;
}

// 函数: calc_stats
// 参数: std::vector<int64_t>& us, int64_t deadline_us
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static Stats calc_stats(std::vector<int64_t>& us, int64_t deadline_us) {
    Stats s;
    if (us.empty()) {
        s.avg_us = s.p99_us = s.max_us = 0.0;
        s.deadline_miss = 0;
        return s;
    }
    int64_t sum = 0;
    int miss = 0;
    for (size_t i = 0; i < us.size(); ++i) {
        sum += us[i];
        if (us[i] > deadline_us) miss++;
    }
    std::sort(us.begin(), us.end());
    s.avg_us = (double)sum / us.size();
    s.p99_us = (double)us[(size_t)(us.size() * 99 / 100)];
    s.max_us = (double)us.back();
    s.deadline_miss = miss;
    return s;
}

// 函数: run_on_demand
// 参数: int frames, size_t frame_bytes, int64_t deadline_us
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static Stats run_on_demand(int frames, size_t frame_bytes, int64_t deadline_us) {
    std::vector<int64_t> lat;
    lat.reserve((size_t)frames);
    volatile uint8_t sink = 0;

    for (int i = 0; i < frames; ++i) {
        int64_t t0 = mono_ns();
        uint8_t* p = (uint8_t*)malloc(frame_bytes);
        if (!p) {
            perror("malloc");
            exit(1);
        }
        // 每页触碰，模拟真实帧写入+触页行为
        touch_all_pages(p, frame_bytes);
        sink ^= p[0];
        free(p);
        int64_t t1 = mono_ns();
        int64_t cost_us = (t1 - t0) / 1000;
        lat.push_back(cost_us);

        // 简单节拍器：若本帧提前完成，sleep 到下一周期
        if (cost_us < deadline_us) {
            usleep((useconds_t)(deadline_us - cost_us));
        }
    }
    (void)sink;
    return calc_stats(lat, deadline_us);
}

// 函数: run_prealloc_prefault
// 参数: int frames, size_t frame_bytes, int ring_count, int64_t deadline_us
// 说明: 见本函数内部逻辑与文件顶部实验说明。
static Stats run_prealloc_prefault(int frames, size_t frame_bytes, int ring_count, int64_t deadline_us) {
    if (ring_count < 2) ring_count = 2;
    std::vector<uint8_t*> ring((size_t)ring_count, (uint8_t*)NULL);
    for (int i = 0; i < ring_count; ++i) {
        ring[(size_t)i] = (uint8_t*)malloc(frame_bytes);
        if (!ring[(size_t)i]) {
            perror("malloc ring");
            exit(1);
        }
    }

    // 预热触页：把潜在 page fault 从实时环路“前移”到初始化阶段
    for (int i = 0; i < ring_count; ++i) {
        touch_all_pages(ring[(size_t)i], frame_bytes);
    }

    // 尝试 mlock：防止这些缓冲被换出（可能受系统 memlock 限制）
    int lock_ok = 1;
    for (int i = 0; i < ring_count; ++i) {
        if (mlock(ring[(size_t)i], frame_bytes) != 0) {
            lock_ok = 0;
            break;
        }
    }

    std::vector<int64_t> lat;
    lat.reserve((size_t)frames);
    volatile uint8_t sink = 0;
    for (int i = 0; i < frames; ++i) {
        int64_t t0 = mono_ns();
        uint8_t* p = ring[(size_t)(i % ring_count)];
        // 模拟每帧写入（这里仍按页触碰）
        touch_all_pages(p, frame_bytes);
        sink ^= p[0];
        int64_t t1 = mono_ns();
        int64_t cost_us = (t1 - t0) / 1000;
        lat.push_back(cost_us);
        if (cost_us < deadline_us) {
            usleep((useconds_t)(deadline_us - cost_us));
        }
    }
    (void)sink;

    for (int i = 0; i < ring_count; ++i) {
        if (lock_ok) {
            munlock(ring[(size_t)i], frame_bytes);
        }
        free(ring[(size_t)i]);
    }

    if (!lock_ok) {
        printf("note: mlock failed (可能是 memlock 限制)，本次结果主要体现预分配+预热收益。\n");
    }
    return calc_stats(lat, deadline_us);
}

// 函数: main
// 参数: int argc, char** argv
// 说明: 见本函数内部逻辑与文件顶部实验说明。
int main(int argc, char** argv) {
    // 参数:
    // [1] frames(default 600)
    // [2] frame_kb(default 3072) 约等于1080p YUV420
    // [3] deadline_us(default 16666) 约等于 60fps 周期
    // [4] ring_count(default 6)
    int frames = arg_or_default(argc, argv, 1, 600);
    int frame_kb = arg_or_default(argc, argv, 2, 3072);
    int deadline_us = arg_or_default(argc, argv, 3, 16666);
    int ring_count = arg_or_default(argc, argv, 4, 6);
    if (frames <= 0) frames = 1;
    if (frame_kb <= 0) frame_kb = 64;
    if (deadline_us <= 0) deadline_us = 1000;
    if (ring_count <= 0) ring_count = 2;

    size_t frame_bytes = (size_t)frame_kb * 1024;
    printf("config: frames=%d frame=%dKB deadline=%dus ring=%d\n",
           frames, frame_kb, deadline_us, ring_count);

    Stats a = run_on_demand(frames, frame_bytes, deadline_us);
    Stats b = run_prealloc_prefault(frames, frame_bytes, ring_count, deadline_us);

    printf("[ON_DEMAND]        avg=%.2fus p99=%.2fus max=%.2fus miss=%d/%d\n",
           a.avg_us, a.p99_us, a.max_us, a.deadline_miss, frames);
    printf("[PREALLOC_PREFAULT] avg=%.2fus p99=%.2fus max=%.2fus miss=%d/%d\n",
           b.avg_us, b.p99_us, b.max_us, b.deadline_miss, frames);

    printf("解读:\n");
    printf("1) PREALLOC_PREFAULT 通常可降低长尾延迟（p99/max）和 deadline miss。\n");
    printf("2) 实时音视频更关注抖动与最坏帧，而不仅是平均值。\n");
    return 0;
}
