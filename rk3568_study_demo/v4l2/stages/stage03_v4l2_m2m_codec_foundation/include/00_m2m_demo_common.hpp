#ifndef M2M_DEMO_COMMON_HPP_
#define M2M_DEMO_COMMON_HPP_

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace m2m_demo {

/*
 * 结构体作用：保存单个 mmap buffer 的用户态映射信息。
 * 生命周期：
 * 1) query+map 后 addr/length 有效。
 * 2) munmap 后 addr 必须置空，避免悬挂指针。
 * 驱动影子线：
 * - 对应 vb2 在内核分配的一个 queue buffer；
 * - 用户态通过 offset + mmap 映射到该 buffer。
 */
struct MappedBuffer {
    void* addr = nullptr;
    size_t length = 0;
    uint32_t index = 0;
};

/*
 * 函数作用：统一 ioctl 调用，并在 EINTR 时自动重试。
 * 输入参数：
 * - fd: 已打开的视频设备句柄。
 * - request: ioctl 命令号，例如 VIDIOC_S_FMT。
 * - arg: 命令参数结构体。
 * 输出结果：返回 ioctl 原始返回值（0 成功，<0 失败）。
 * 失败路径：
 * - 非 EINTR 错误将直接返回，调用方读取 errno 定位。
 * 驱动影子线：
 * - 每次 xioctl 都会进入 V4L2 驱动对应的 ioctl 回调。
 */
inline int xioctl(int fd, unsigned long request, void* arg) {
    int ret;
    do {
        // EINTR 表示系统调用被信号中断，媒体应用里应重试而不是立即失败。
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

/*
 * 函数作用：确保目录存在。
 * 工作场景：输出日志/报告文件前创建目录。
 */
inline bool ensure_dir(const std::string& path) {
    // mkdir 成功或目录已存在都视为可继续。
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) {
        return true;
    }
    fprintf(stderr, "mkdir failed: %s: %s\n", path.c_str(), strerror(errno));
    return false;
}

/*
 * 函数作用：解析 --key=value 形式参数。
 * 返回值：
 * - true: 当前参数匹配 key，并把 value 写入 out。
 * - false: 不匹配该 key。
 */
inline bool parse_kv(const char* arg, const char* key, std::string* out) {
    const size_t n = strlen(key);
    // 既要前缀匹配 key，还要紧跟 '='，避免把 --devx 误判为 --dev。
    if (strncmp(arg, key, n) != 0 || arg[n] != '=') {
        return false;
    }
    *out = arg + n + 1;
    return true;
}

/*
 * 函数作用：解析整数参数。
 * 说明：教学 demo 中使用 atoi，生产代码建议改成更严格的 strtol 校验。
 */
inline bool parse_kv_int(const char* arg, const char* key, int* out) {
    std::string value;
    if (!parse_kv(arg, key, &value)) {
        return false;
    }
    *out = atoi(value.c_str());
    return true;
}

/*
 * 函数作用：解析无符号 32 位整数参数。
 * 失败条件：
 * - 不是该 key；
 * - 解析后为负数（与 u32 语义冲突）。
 */
inline bool parse_kv_u32(const char* arg, const char* key, uint32_t* out) {
    int tmp = 0;
    if (!parse_kv_int(arg, key, &tmp)) {
        return false;
    }
    if (tmp < 0) {
        return false;
    }
    *out = static_cast<uint32_t>(tmp);
    return true;
}

/*
 * 函数作用：把 4 字符编码转成 v4l2 fourcc。
 * 失败条件：字符串长度不是 4。
 */
inline bool parse_fourcc(const std::string& s, uint32_t* out) {
    if (s.size() != 4) {
        return false;
    }
    *out = v4l2_fourcc(s[0], s[1], s[2], s[3]);
    return true;
}

/*
 * 函数作用：把 fourcc 转回可读字符串。
 * 工作场景：日志打印格式协商结果。
 */
inline std::string fourcc_to_string(uint32_t f) {
    char s[5];
    s[0] = static_cast<char>(f & 0xff);
    s[1] = static_cast<char>((f >> 8) & 0xff);
    s[2] = static_cast<char>((f >> 16) & 0xff);
    s[3] = static_cast<char>((f >> 24) & 0xff);
    s[4] = '\0';
    return std::string(s);
}

/*
 * 函数作用：打开视频节点。
 * 输入假设：dev 指向合法节点路径（如 /dev/video0）。
 * 关键选择：
 * - O_NONBLOCK 让后续 DQBUF/poll 更容易构建非阻塞状态机。
 * 失败现象：
 * - ENOENT: 节点不存在；
 * - EACCES: 权限不足。
 */
inline bool open_node(const std::string& dev, int* out_fd) {
    const int fd = open(dev.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", dev.c_str(), strerror(errno));
        return false;
    }
    *out_fd = fd;
    return true;
}

/*
 * 函数作用：查询设备能力。
 * 状态变化：成功后 cap 字段可用于判断是否 M2M、支持哪些能力位。
 */
inline bool querycap(int fd, v4l2_capability* cap) {
    // 先清零，避免读到旧内存脏值。
    memset(cap, 0, sizeof(*cap));
    if (xioctl(fd, VIDIOC_QUERYCAP, cap) < 0) {
        fprintf(stderr, "VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

/*
 * 函数作用：根据 mplane 开关给出 OUTPUT 队列 type。
 * 驱动影子线：
 * - type 会决定驱动走单平面还是多平面的 vb2 路径。
 */
inline uint32_t output_type_from_mplane(int mplane) {
    return mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

/*
 * 函数作用：根据 mplane 开关给出 CAPTURE 队列 type。
 */
inline uint32_t capture_type_from_mplane(int mplane) {
    return mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

/*
 * 函数作用：对指定队列执行 VIDIOC_S_FMT。
 * 输入参数：
 * - type: OUTPUT 或 CAPTURE（单平面/多平面）。
 * - fourcc/width/height: 请求格式。
 * 预期状态变化：
 * - 驱动完成格式协商，可能回填实际 width/height/stride/sizeimage。
 * 失败现象：
 * - 常见 EINVAL，表示格式或尺寸不支持。
 * 驱动影子线：
 * - 会进入驱动 s_fmt 回调，通常伴随硬件上下文参数更新。
 */
inline bool set_format(int fd, uint32_t type, uint32_t fourcc, uint32_t width,
                       uint32_t height, bool verbose) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = type;

    if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
        // 多平面路径：pix_mp + num_planes。
        fmt.fmt.pix_mp.width = width;
        fmt.fmt.pix_mp.height = height;
        fmt.fmt.pix_mp.pixelformat = fourcc;
        fmt.fmt.pix_mp.num_planes = 1;
    } else {
        // 单平面路径：pix。
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = fourcc;
    }

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "VIDIOC_S_FMT(type=%u fourcc=%s) failed: %s\n", type,
                fourcc_to_string(fourcc).c_str(), strerror(errno));
        return false;
    }

    if (verbose) {
        if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
            printf("S_FMT type=%u ok: %ux%u fourcc=%s planes=%u\n", type,
                   fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
                   fourcc_to_string(fmt.fmt.pix_mp.pixelformat).c_str(),
                   fmt.fmt.pix_mp.num_planes);
        } else {
            printf("S_FMT type=%u ok: %ux%u fourcc=%s bytesperline=%u sizeimage=%u\n",
                   type, fmt.fmt.pix.width, fmt.fmt.pix.height,
                   fourcc_to_string(fmt.fmt.pix.pixelformat).c_str(),
                   fmt.fmt.pix.bytesperline, fmt.fmt.pix.sizeimage);
        }
    }
    return true;
}

/*
 * 函数作用：请求驱动分配 queue buffer（VIDIOC_REQBUFS）。
 * 参数语义：
 * - count: 请求数量；驱动可缩减，真实值写入 granted_count。
 * 状态机关系：
 * - 通常在 S_FMT 之后调用。
 * 驱动影子线：
 * - 对应 vb2 queue_setup/alloc 路径。
 */
inline bool reqbufs(int fd, uint32_t type, uint32_t memory, uint32_t count,
                    uint32_t* granted_count, bool verbose) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = count;
    req.type = type;
    req.memory = memory;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr, "VIDIOC_REQBUFS(type=%u, count=%u) failed: %s\n", type,
                count, strerror(errno));
        return false;
    }

    *granted_count = req.count;
    if (verbose) {
        printf("REQBUFS type=%u ok: requested=%u granted=%u\n", type, count,
               req.count);
    }
    return true;
}

/*
 * 函数作用：对单平面 buffer 执行 QUERYBUF + mmap。
 * 前置条件：
 * - REQBUFS 已成功；
 * - index 小于 granted_count。
 * 资源所有权：
 * - 成功后 out->addr/out->length 生效；调用方必须最终 munmap。
 * 失败路径：
 * - QUERYBUF 失败：通常为参数/状态机错误；
 * - mmap 失败：映射失败，不会持有半初始化资源。
 */
inline bool querybuf_map_single_planar(int fd, uint32_t type, uint32_t index,
                                       MappedBuffer* out, bool verbose) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;

    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        fprintf(stderr, "VIDIOC_QUERYBUF(type=%u idx=%u) failed: %s\n", type, index,
                strerror(errno));
        return false;
    }

    // 使用驱动返回的 offset/length 做 mmap，建立共享映射。
    void* addr = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                      buf.m.offset);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "mmap(type=%u idx=%u) failed: %s\n", type, index,
                strerror(errno));
        return false;
    }

    out->addr = addr;
    out->length = buf.length;
    out->index = index;

    if (verbose) {
        printf("QUERYBUF+MMAP type=%u idx=%u ok: length=%u offset=%u\n", type, index,
               buf.length, buf.m.offset);
    }
    return true;
}

/*
 * 函数作用：QBUF（用户态 -> 驱动态）。
 * 所有权方向：
 * - 成功后该 buffer 交给驱动，用户态不应改写其“在驱动持有期间”的有效内容。
 * 参数语义：
 * - bytesused: OUTPUT 队列时通常表示有效码流长度；
 *              CAPTURE 队列时常为 0（空帧缓冲供驱动填充）。
 */
inline bool qbuf_single_planar(int fd, uint32_t type, uint32_t index,
                               uint32_t bytesused, bool verbose) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    buf.bytesused = bytesused;

    if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr, "VIDIOC_QBUF(type=%u idx=%u bytesused=%u) failed: %s\n", type,
                index, bytesused, strerror(errno));
        return false;
    }
    if (verbose) {
        printf("QBUF type=%u idx=%u bytesused=%u ok\n", type, index, bytesused);
    }
    return true;
}

/*
 * 函数作用：DQBUF（驱动态 -> 用户态）。
 * 所有权方向：
 * - 成功后该 buffer 的所有权返回用户态，可读取 bytesused/flags/sequence。
 * 失败行为：
 * - 直接返回 false，调用方可根据 errno 区分 EAGAIN 与致命错误。
 */
inline bool dqbuf_single_planar(int fd, uint32_t type, v4l2_buffer* out,
                                bool verbose) {
    memset(out, 0, sizeof(*out));
    out->type = type;
    out->memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_DQBUF, out) < 0) {
        return false;
    }

    if (verbose) {
        printf("DQBUF type=%u idx=%u bytesused=%u seq=%u flags=0x%x\n", out->type,
               out->index, out->bytesused, out->sequence, out->flags);
    }
    return true;
}

/*
 * 函数作用：开启队列 STREAMON。
 * 前置条件：
 * - 通常至少已有若干 QBUF。
 * 驱动影子线：
 * - 驱动侧状态机从 idle 转到 streaming。
 */
inline bool stream_on(int fd, uint32_t type, bool verbose) {
    enum v4l2_buf_type t = static_cast<enum v4l2_buf_type>(type);
    if (xioctl(fd, VIDIOC_STREAMON, &t) < 0) {
        fprintf(stderr, "VIDIOC_STREAMON(type=%u) failed: %s\n", type,
                strerror(errno));
        return false;
    }
    if (verbose) {
        printf("STREAMON type=%u ok\n", type);
    }
    return true;
}

/*
 * 函数作用：best-effort 关闭队列 STREAMOFF。
 * 设计说明：
 * - 清理路径里即使 STREAMOFF 失败，也尽量继续后续资源回收。
 */
inline void stream_off_best_effort(int fd, uint32_t type, bool verbose) {
    enum v4l2_buf_type t = static_cast<enum v4l2_buf_type>(type);
    if (xioctl(fd, VIDIOC_STREAMOFF, &t) == 0) {
        if (verbose) {
            printf("STREAMOFF type=%u ok\n", type);
        }
    }
}

/*
 * 函数作用：poll 等待设备可读事件。
 * 返回值：
 * - >0: 有事件；
 * - 0: timeout；
 * - <0: poll 失败。
 * 驱动影子线：
 * - 常见由中断完成或状态事件唤醒。
 */
inline int poll_readable(int fd, int timeout_ms, bool verbose) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLPRI;
    pfd.revents = 0;

    const int ret = poll(&pfd, 1, timeout_ms);
    if (verbose) {
        printf("poll ret=%d revents=0x%x timeout_ms=%d\n", ret, pfd.revents,
               timeout_ms);
    }
    return ret;
}

/*
 * 函数作用：批量 munmap 并清空容器。
 * 资源对称性：
 * - 每个 querybuf_map 成功的 buffer 必须在退出前调用 unmap。
 */
inline void unmap_all(std::vector<MappedBuffer>* buffers, bool verbose) {
    for (size_t i = 0; i < buffers->size(); ++i) {
        MappedBuffer& b = (*buffers)[i];
        if (b.addr && b.length > 0) {
            munmap(b.addr, b.length);
            if (verbose) {
                printf("munmap idx=%u length=%zu\n", b.index, b.length);
            }
            b.addr = nullptr;
            b.length = 0;
        }
    }
    buffers->clear();
}

}  // namespace m2m_demo

#endif  // M2M_DEMO_COMMON_HPP_
