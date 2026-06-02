#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "stage01_v4l2_capture.hpp"
#include "stage01_v4l2_common.hpp"
#include "stage01_v4l2_types.hpp"
#include "stage01_v4l2_stats.hpp"

using ::testing::Return;
using ::testing::_;

namespace stage01_v4l2 {

// Mock实现，用于模拟系统调用
class MockSysCalls {
public:
    static int (*real_open)(const char *, int);
    static int (*real_ioctl)(int, unsigned long, void *);
    static void* (*real_mmap)(void *, size_t, int, int, int, off_t);
    static int (*real_munmap)(void *, size_t);
    static int (*real_close)(int);
    static int (*real_select)(int, fd_set*, fd_set*, fd_set*, struct timeval*);

    static int mock_open(const char *pathname, int flags) {
        return real_open ? real_open(pathname, flags) : 0;
    }

    static int mock_ioctl(int fd, unsigned long request, void *argp) {
        return real_ioctl ? real_ioctl(fd, request, argp) : 0;
    }

    static void* mock_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
        return real_mmap ? real_mmap(addr, length, prot, flags, fd, offset) : MAP_FAILED;
    }

    static int mock_munmap(void *addr, size_t length) {
        return real_munmap ? real_munmap(addr, length) : 0;
    }

    static int mock_close(int fd) {
        return real_close ? real_close(fd) : 0;
    }

    static int mock_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
        return real_select ? real_select(nfds, readfds, writefds, exceptfds, timeout) : 1;
    }
};

// 初始化静态函数指针
int (*MockSysCalls::real_open)(const char *, int) = nullptr;
int (*MockSysCalls::real_ioctl)(int, unsigned long, void *) = nullptr;
void* (*MockSysCalls::real_mmap)(void *, size_t, int, int, int, off_t) = nullptr;
int (*MockSysCalls::real_munmap)(void *, size_t) = nullptr;
int (*MockSysCalls::real_close)(int) = nullptr;
int (*MockSysCalls::real_select)(int, fd_set*, fd_set*, fd_set*, struct timeval*) = nullptr;

// 测试dump_supported_formats_and_intervals函数
TEST(V4L2CaptureTest, DumpSupportedFormatsAndIntervals) {
    // 由于这个函数主要是打印信息，难以直接测试，我们只验证其不会崩溃
    int fake_fd = 123;
    EXPECT_NO_THROW(dump_supported_formats_and_intervals(fake_fd));
}

// 测试xioctl函数
TEST(V4L2CaptureTest, XioctlSuccess) {
    int fd = 123;
    int request = VIDIOC_QUERYCAP;
    v4l2_capability cap;
    
    // 设置临时ioctl函数来模拟成功
    int (*original_ioctl)(int, unsigned long, void *) = ioctl;
    MockSysCalls::real_ioctl = [](int fd, unsigned long request, void *argp) -> int {
        v4l2_capability *cap = static_cast<v4l2_capability*>(argp);
        strcpy(reinterpret_cast<char*>(cap->driver), "test_driver");
        return 0; // 成功
    };
    
    EXPECT_EQ(xioctl(fd, request, &cap), 0);
    
    // 恢复原始ioctl函数
    MockSysCalls::real_ioctl = original_ioctl;
}

TEST(V4L2CaptureTest, XioctlEINTRRetry) {
    int fd = 123;
    int request = VIDIOC_QUERYCAP;
    v4l2_capability cap;
    
    int call_count = 0;
    int (*original_ioctl)(int, unsigned long, void *) = ioctl;
    MockSysCalls::real_ioctl = [&call_count](int fd, unsigned long request, void *argp) -> int {
        call_count++;
        if (call_count == 1) {
            errno = EINTR;
            return -1; // 模拟EINTR
        }
        v4l2_capability *cap = static_cast<v4l2_capability*>(argp);
        strcpy(reinterpret_cast<char*>(cap->driver), "test_driver");
        return 0; // 成功
    };
    
    EXPECT_EQ(xioctl(fd, request, &cap), 0);
    EXPECT_EQ(call_count, 2); // 应该重试一次
    
    // 恢复原始ioctl函数
    MockSysCalls::real_ioctl = original_ioctl;
}

// 测试CaptureRunResult构造函数
TEST(V4L2CaptureTest, CaptureRunResultConstructor) {
    CaptureRunResult result;
    
    EXPECT_EQ(result.ret_code, 1); // 默认值
    EXPECT_EQ(result.saved_bytes, 0);
    EXPECT_EQ(result.saved_seq, 0);
    // 检查stats是否被初始化
    EXPECT_EQ(result.stats.select_calls, 0);
    EXPECT_EQ(result.stats.dq_ok, 0);
    EXPECT_EQ(result.stats.requeue_ok, 0);
}

// 测试fourcc_to_string函数
TEST(V4L2CaptureTest, FourCCToString) {
    EXPECT_EQ(fourcc_to_string(V4L2_PIX_FMT_YUYV), "YUYV");
    EXPECT_EQ(fourcc_to_string(V4L2_PIX_FMT_MJPEG), "MJPG");
}

// 测试now_ms函数
TEST(V4L2CaptureTest, NowMsReturnsValidValue) {
    double time1 = now_ms();
    usleep(1000); // 睡眠1毫秒
    double time2 = now_ms();
    
    EXPECT_LE(time1, time2); // 第二个时间应该大于或等于第一个
}

// 测试init_stats函数
TEST(V4L2CaptureTest, InitStats) {
    CaptureStats stats;
    init_stats(&stats);
    
    EXPECT_EQ(stats.select_calls, 0);
    EXPECT_EQ(stats.dq_ok, 0);
    EXPECT_EQ(stats.bytes_min, 0xFFFFFFFFU);
    EXPECT_EQ(stats.bytes_max, 0U);
    EXPECT_TRUE(stats.bytes_hist.empty());
    EXPECT_FALSE(stats.has_last_sequence);
    EXPECT_EQ(stats.last_sequence, 0);
    EXPECT_EQ(stats.sequence_gap_frames, 0);
    EXPECT_TRUE(stats.flags_hist.empty());
    EXPECT_FALSE(stats.has_last_dq_host_ms);
    EXPECT_EQ(stats.v4l2_ts_zero_count, 0);
}

// 测试update_bytes_hist函数
TEST(V4L2CaptureTest, UpdateBytesHist) {
    CaptureStats stats;
    init_stats(&stats);
    
    update_bytes_hist(&stats, 100);
    update_bytes_hist(&stats, 200);
    update_bytes_hist(&stats, 50);
    
    EXPECT_EQ(stats.bytes_total, 350);
    EXPECT_EQ(stats.bytes_min, 50);
    EXPECT_EQ(stats.bytes_max, 200);
    EXPECT_EQ(stats.bytes_hist[100], 1);
    EXPECT_EQ(stats.bytes_hist[200], 1);
    EXPECT_EQ(stats.bytes_hist[50], 1);
}

// 测试update_sequence_gap函数
TEST(V4L2CaptureTest, UpdateSequenceGap) {
    CaptureStats stats;
    init_stats(&stats);
    
    update_sequence_gap(&stats, 100);
    EXPECT_EQ(stats.sequence_gap_frames, 0);
    EXPECT_TRUE(stats.has_last_sequence);
    EXPECT_EQ(stats.last_sequence, 100);
    
    update_sequence_gap(&stats, 103); // 跳过了2帧
    EXPECT_EQ(stats.sequence_gap_frames, 2);
    EXPECT_EQ(stats.last_sequence, 103);
}

// 创建一个简单的AppConfig实例进行测试
TEST(V4L2CaptureTest, AppConfigInitialization) {
    AppConfig cfg;
    
    cfg.dev = "/dev/video0";
    cfg.req_width = 640;
    cfg.req_height = 480;
    cfg.total_frames = 30;
    cfg.warmup_frames = 5;
    cfg.req_fps = 30;
    cfg.timeout_ms = 2000;
    cfg.req_buf_count = 4;
    cfg.save_preview = false;
    cfg.dump_formats = false;
    cfg.log_every = 10;
    
    EXPECT_EQ(cfg.dev, "/dev/video0");
    EXPECT_EQ(cfg.req_width, 640);
    EXPECT_EQ(cfg.req_height, 480);
    EXPECT_EQ(cfg.total_frames, 30);
}

} // namespace stage01_v4l2