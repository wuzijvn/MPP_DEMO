#ifndef STAGE06_M2M_COMMON_HPP_
#define STAGE06_M2M_COMMON_HPP_

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
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace stage06 {

/*
 * Stage06 公共工具头。
 *
 * 本阶段重构后的边界：
 * - VM track 使用 vim2m 这类虚拟 V4L2 M2M 节点真实执行 ioctl/queue/mmap/poll。
 * - RK track 使用 RKMPP/FFmpeg/板端命令收集硬件路径证据，不把 ISP/camera 节点伪装成 codec M2M。
 * - 这里放低层 V4L2 helper；核心学习流程仍放在各 demo 文件里，方便按文件阅读。
 *
 * 驱动影子线：
 * - open 对应 driver file_operations.open。
 * - S_FMT/REQBUFS/QUERYBUF/QBUF/DQBUF/STREAMON/STREAMOFF 对应 V4L2 ioctl ops、vb2 queue 和 v4l2-mem2mem 调度。
 * - poll 对应驱动 waitqueue/wakeup，真实硬件上常由 IRQ 或 worker completion 唤醒。
 */

struct MappedBuffer {
    void* addr = nullptr;
    size_t length = 0;
    uint32_t index = 0;
};

struct QueueCounters {
    int qbuf_output = 0;
    int qbuf_capture = 0;
    int dqbuf_output = 0;
    int dqbuf_capture = 0;
    int poll_calls = 0;
    int timeout_count = 0;
    int streamon_count = 0;
    int streamoff_count = 0;
    int mapped_output = 0;
    int mapped_capture = 0;
    int bytesused_zero_count = 0;
};

inline int xioctl(int fd, unsigned long request, void* arg) {
    int ret;
    do {
        /*
         * EINTR 表示系统调用被信号打断。
         * 媒体程序中的 ioctl 通常处于状态机中，遇到 EINTR 应重试，避免制造假失败。
         */
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

inline bool ensure_dir(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) {
        return true;
    }
    std::cerr << "mkdir failed: " << path << ": " << strerror(errno) << "\n";
    return false;
}

inline bool has_arg(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (key == argv[i]) {
            return true;
        }
    }
    return false;
}

inline std::string get_arg(int argc, char** argv, const std::string& key,
                           const std::string& def) {
    const std::string prefix = key + "=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.compare(0, prefix.size(), prefix) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return def;
}

inline int get_arg_int(int argc, char** argv, const std::string& key, int def) {
    const std::string value = get_arg(argc, argv, key, "");
    if (value.empty()) {
        return def;
    }
    return atoi(value.c_str());
}

inline uint32_t get_arg_u32(int argc, char** argv, const std::string& key,
                            uint32_t def) {
    const int value = get_arg_int(argc, argv, key, static_cast<int>(def));
    if (value < 0) {
        return def;
    }
    return static_cast<uint32_t>(value);
}

inline bool parse_fourcc(const std::string& text, uint32_t* out) {
    if (text.size() != 4 || out == NULL) {
        return false;
    }
    *out = v4l2_fourcc(text[0], text[1], text[2], text[3]);
    return true;
}

inline std::string fourcc_to_string(uint32_t fourcc) {
    char s[5];
    s[0] = static_cast<char>(fourcc & 0xff);
    s[1] = static_cast<char>((fourcc >> 8) & 0xff);
    s[2] = static_cast<char>((fourcc >> 16) & 0xff);
    s[3] = static_cast<char>((fourcc >> 24) & 0xff);
    s[4] = '\0';
    return std::string(s);
}

inline std::string yes_no(bool v) {
    return v ? "yes" : "no";
}

inline std::string hex_u32(uint32_t value) {
    std::ostringstream oss;
    oss << "0x" << std::hex << value;
    return oss.str();
}

inline void print_line(const std::string& tag, const std::string& message) {
    std::cout << std::left << std::setw(24) << tag << " : " << message << "\n";
}

inline void print_counter(const std::string& name, int value) {
    std::cout << std::left << std::setw(32) << name << " = " << value << "\n";
}

inline bool write_text_file(const std::string& path, const std::string& body) {
    FILE* fp = fopen(path.c_str(), "w");
    if (fp == NULL) {
        std::cerr << "fopen " << path << " failed: " << strerror(errno) << "\n";
        return false;
    }
    const size_t written = fwrite(body.data(), 1, body.size(), fp);
    fclose(fp);
    if (written != body.size()) {
        std::cerr << "write incomplete: " << path << "\n";
        return false;
    }
    return true;
}

inline uint32_t output_type(bool mplane) {
    return mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

inline uint32_t capture_type(bool mplane) {
    return mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

inline std::string buffer_type_name(uint32_t type) {
    switch (type) {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
            return "VIDEO_OUTPUT";
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
            return "VIDEO_CAPTURE";
        case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
            return "VIDEO_OUTPUT_MPLANE";
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
            return "VIDEO_CAPTURE_MPLANE";
        default:
            return "UNKNOWN_TYPE";
    }
}

inline uint32_t active_caps(const struct v4l2_capability& cap) {
    return (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
}

inline bool is_m2m_capable(uint32_t caps) {
    return (caps & V4L2_CAP_VIDEO_M2M) || (caps & V4L2_CAP_VIDEO_M2M_MPLANE);
}

inline bool is_streaming_capable(uint32_t caps) {
    return (caps & V4L2_CAP_STREAMING) != 0;
}

inline bool open_video_node(const std::string& device, int* out_fd) {
    if (out_fd == NULL) {
        return false;
    }
    /*
     * O_NONBLOCK 是 V4L2 测试工具常用选择：
     * - DQBUF 没有 ready buffer 时返回 EAGAIN；
     * - 程序用 poll 决定何时再 DQBUF；
     * - 这正好映射到驱动 waitqueue/wakeup 机制。
     */
    const int fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "open " << device << " failed: " << strerror(errno) << "\n";
        return false;
    }
    *out_fd = fd;
    return true;
}

inline bool query_capability(int fd, struct v4l2_capability* cap) {
    if (cap == NULL) {
        return false;
    }
    memset(cap, 0, sizeof(*cap));
    if (xioctl(fd, VIDIOC_QUERYCAP, cap) < 0) {
        std::cerr << "VIDIOC_QUERYCAP failed: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

inline void print_capability(const struct v4l2_capability& cap) {
    const uint32_t caps = active_caps(cap);
    print_line("driver", reinterpret_cast<const char*>(cap.driver));
    print_line("card", reinterpret_cast<const char*>(cap.card));
    print_line("bus_info", reinterpret_cast<const char*>(cap.bus_info));
    print_line("active_caps_hex", hex_u32(caps));
    print_line("m2m_capable", yes_no(is_m2m_capable(caps)));
    print_line("streaming_capable", yes_no(is_streaming_capable(caps)));
}

inline bool enum_formats(int fd, uint32_t type, std::vector<uint32_t>* formats,
                         bool verbose) {
    if (formats == NULL) {
        return false;
    }
    formats->clear();
    for (uint32_t index = 0;; ++index) {
        struct v4l2_fmtdesc desc;
        memset(&desc, 0, sizeof(desc));
        desc.index = index;
        desc.type = type;
        if (xioctl(fd, VIDIOC_ENUM_FMT, &desc) < 0) {
            if (errno == EINVAL) {
                break;
            }
            std::cerr << "VIDIOC_ENUM_FMT " << buffer_type_name(type)
                      << " failed: " << strerror(errno) << "\n";
            return false;
        }
        formats->push_back(desc.pixelformat);
        if (verbose) {
            std::cout << "  [" << index << "] " << fourcc_to_string(desc.pixelformat)
                      << " - " << reinterpret_cast<const char*>(desc.description) << "\n";
        }
    }
    return true;
}

inline bool fill_format(struct v4l2_format* fmt, uint32_t type, uint32_t fourcc,
                        uint32_t width, uint32_t height) {
    if (fmt == NULL) {
        return false;
    }
    memset(fmt, 0, sizeof(*fmt));
    fmt->type = type;
    if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
        fmt->fmt.pix_mp.width = width;
        fmt->fmt.pix_mp.height = height;
        fmt->fmt.pix_mp.pixelformat = fourcc;
        fmt->fmt.pix_mp.num_planes = 1;
    } else {
        fmt->fmt.pix.width = width;
        fmt->fmt.pix.height = height;
        fmt->fmt.pix.pixelformat = fourcc;
    }
    return true;
}

inline void print_format_result(const std::string& title,
                                const struct v4l2_format& fmt) {
    std::cout << title << "\n";
    print_line("type", buffer_type_name(fmt.type));
    if (V4L2_TYPE_IS_MULTIPLANAR(fmt.type)) {
        print_line("width", std::to_string(fmt.fmt.pix_mp.width));
        print_line("height", std::to_string(fmt.fmt.pix_mp.height));
        print_line("fourcc", fourcc_to_string(fmt.fmt.pix_mp.pixelformat));
        print_line("num_planes", std::to_string(fmt.fmt.pix_mp.num_planes));
        for (uint32_t i = 0; i < fmt.fmt.pix_mp.num_planes && i < VIDEO_MAX_PLANES; ++i) {
            std::ostringstream oss;
            oss << "sizeimage=" << fmt.fmt.pix_mp.plane_fmt[i].sizeimage
                << ", bytesperline=" << fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
            print_line("plane" + std::to_string(i), oss.str());
        }
    } else {
        print_line("width", std::to_string(fmt.fmt.pix.width));
        print_line("height", std::to_string(fmt.fmt.pix.height));
        print_line("fourcc", fourcc_to_string(fmt.fmt.pix.pixelformat));
        print_line("bytesperline", std::to_string(fmt.fmt.pix.bytesperline));
        print_line("sizeimage", std::to_string(fmt.fmt.pix.sizeimage));
    }
}

inline bool try_or_set_format(int fd, uint32_t type, uint32_t fourcc,
                              uint32_t width, uint32_t height, bool apply,
                              struct v4l2_format* out_fmt, bool verbose) {
    struct v4l2_format fmt;
    fill_format(&fmt, type, fourcc, width, height);
    /*
     * TRY_FMT 只探测驱动会如何接受/调整格式，不改变 streaming 状态。
     * S_FMT 真正设置队列格式，真实工具通常在 STREAMOFF 且 buffer 未申请时调用。
     */
    const unsigned long request = apply ? VIDIOC_S_FMT : VIDIOC_TRY_FMT;
    if (xioctl(fd, request, &fmt) < 0) {
        std::cerr << (apply ? "VIDIOC_S_FMT" : "VIDIOC_TRY_FMT") << " "
                  << buffer_type_name(type) << " fourcc=" << fourcc_to_string(fourcc)
                  << " failed: " << strerror(errno) << "\n";
        return false;
    }
    if (out_fmt != NULL) {
        *out_fmt = fmt;
    }
    if (verbose) {
        print_format_result(apply ? "S_FMT result:" : "TRY_FMT result:", fmt);
    }
    return true;
}

inline bool request_buffers(int fd, uint32_t type, uint32_t count,
                            uint32_t* granted_count, bool verbose) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = count;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;
    /*
     * REQBUFS 成功后，驱动/vb2 为该 queue 准备 buffer 槽位。
     * 驱动可以调整实际数量，所以必须读取 req.count。
     */
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        std::cerr << "VIDIOC_REQBUFS " << buffer_type_name(type)
                  << " failed: " << strerror(errno) << "\n";
        return false;
    }
    if (granted_count != NULL) {
        *granted_count = req.count;
    }
    if (verbose) {
        print_line("REQBUFS " + buffer_type_name(type),
                   "requested=" + std::to_string(count) + ", granted=" + std::to_string(req.count));
    }
    return true;
}

inline bool release_buffers_best_effort(int fd, uint32_t type, bool verbose) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 0;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        if (verbose) {
            std::cerr << "REQBUFS count=0 " << buffer_type_name(type)
                      << " ignored: " << strerror(errno) << "\n";
        }
        return false;
    }
    return true;
}

inline bool querybuf_map(int fd, uint32_t type, uint32_t index,
                         MappedBuffer* out, bool verbose) {
    if (out == NULL) {
        return false;
    }
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    /*
     * QUERYBUF 返回驱动侧 buffer 的 length 和 mmap offset。
     * mmap 成功后，用户态获得共享映射；最终必须 munmap。
     */
    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        std::cerr << "VIDIOC_QUERYBUF " << buffer_type_name(type)
                  << " index=" << index << " failed: " << strerror(errno) << "\n";
        return false;
    }
    void* addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                      buf.m.offset);
    if (addr == MAP_FAILED) {
        std::cerr << "mmap " << buffer_type_name(type)
                  << " index=" << index << " failed: " << strerror(errno) << "\n";
        return false;
    }
    out->addr = addr;
    out->length = buf.length;
    out->index = index;
    if (verbose) {
        print_line("MMAP " + buffer_type_name(type),
                   "index=" + std::to_string(index) + ", length=" + std::to_string(buf.length));
    }
    return true;
}

inline void unmap_all(std::vector<MappedBuffer>* buffers, bool verbose) {
    if (buffers == NULL) {
        return;
    }
    for (size_t i = 0; i < buffers->size(); ++i) {
        MappedBuffer& b = (*buffers)[i];
        if (b.addr != NULL && b.length > 0) {
            munmap(b.addr, b.length);
            if (verbose) {
                print_line("munmap", "index=" + std::to_string(b.index) +
                                      ", length=" + std::to_string(b.length));
            }
            b.addr = NULL;
            b.length = 0;
        }
    }
    buffers->clear();
}

inline void fill_pattern(MappedBuffer* buffer, uint32_t frame_index) {
    if (buffer == NULL || buffer->addr == NULL || buffer->length == 0) {
        return;
    }
    /*
     * vim2m 是 raw-to-raw M2M 设备，不解析 H.264/H.265。
     * 这里填充可变 pattern，是为了让 OUTPUT buffer 有真实 bytesused 和写入行为。
     */
    unsigned char* p = static_cast<unsigned char*>(buffer->addr);
    for (size_t i = 0; i < buffer->length; ++i) {
        p[i] = static_cast<unsigned char>((i + frame_index * 17u) & 0xffu);
    }
}

inline bool qbuf(int fd, uint32_t type, uint32_t index, uint32_t bytesused,
                 bool verbose) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    buf.bytesused = bytesused;
    /*
     * QBUF 成功后，buffer 所有权 USER -> DRIVER。
     * OUTPUT bytesused 表示有效输入数据长度；CAPTURE 一般传 0，表示空帧缓冲。
     */
    if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        std::cerr << "VIDIOC_QBUF " << buffer_type_name(type)
                  << " index=" << index << " bytesused=" << bytesused
                  << " failed: " << strerror(errno) << "\n";
        return false;
    }
    if (verbose) {
        print_line("QBUF " + buffer_type_name(type),
                   "index=" + std::to_string(index) + ", bytesused=" + std::to_string(bytesused));
    }
    return true;
}

inline bool dqbuf(int fd, uint32_t type, struct v4l2_buffer* out,
                  bool verbose) {
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->type = type;
    out->memory = V4L2_MEMORY_MMAP;
    /*
     * DQBUF 成功后，buffer 所有权 DRIVER -> USER。
     * EAGAIN 表示非阻塞模式下暂时没有完成 buffer，调用方通常继续 poll。
     */
    if (xioctl(fd, VIDIOC_DQBUF, out) < 0) {
        return false;
    }
    if (verbose) {
        std::ostringstream oss;
        oss << "index=" << out->index
            << ", bytesused=" << out->bytesused
            << ", sequence=" << out->sequence
            << ", flags=0x" << std::hex << out->flags;
        print_line("DQBUF " + buffer_type_name(type), oss.str());
    }
    return true;
}

inline bool stream_on(int fd, uint32_t type, bool verbose) {
    enum v4l2_buf_type t = static_cast<enum v4l2_buf_type>(type);
    if (xioctl(fd, VIDIOC_STREAMON, &t) < 0) {
        std::cerr << "VIDIOC_STREAMON " << buffer_type_name(type)
                  << " failed: " << strerror(errno) << "\n";
        return false;
    }
    if (verbose) {
        print_line("STREAMON", buffer_type_name(type));
    }
    return true;
}

inline bool stream_off(int fd, uint32_t type, bool verbose) {
    enum v4l2_buf_type t = static_cast<enum v4l2_buf_type>(type);
    if (xioctl(fd, VIDIOC_STREAMOFF, &t) < 0) {
        std::cerr << "VIDIOC_STREAMOFF " << buffer_type_name(type)
                  << " failed: " << strerror(errno) << "\n";
        return false;
    }
    if (verbose) {
        print_line("STREAMOFF", buffer_type_name(type));
    }
    return true;
}

inline void stream_off_best_effort(int fd, uint32_t type, bool verbose) {
    enum v4l2_buf_type t = static_cast<enum v4l2_buf_type>(type);
    if (xioctl(fd, VIDIOC_STREAMOFF, &t) == 0 && verbose) {
        print_line("STREAMOFF", buffer_type_name(type));
    }
}

inline int poll_device(int fd, int timeout_ms, bool verbose) {
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT | POLLPRI;
    const int ret = poll(&pfd, 1, timeout_ms);
    if (verbose) {
        std::ostringstream oss;
        oss << "ret=" << ret << ", revents=0x" << std::hex << pfd.revents
            << ", timeout_ms=" << std::dec << timeout_ms;
        print_line("poll", oss.str());
    }
    return ret;
}

inline void print_counters(const QueueCounters& c) {
    print_counter("qbuf_output", c.qbuf_output);
    print_counter("qbuf_capture", c.qbuf_capture);
    print_counter("dqbuf_output", c.dqbuf_output);
    print_counter("dqbuf_capture", c.dqbuf_capture);
    print_counter("poll_calls", c.poll_calls);
    print_counter("timeout_count", c.timeout_count);
    print_counter("streamon_count", c.streamon_count);
    print_counter("streamoff_count", c.streamoff_count);
    print_counter("mapped_output", c.mapped_output);
    print_counter("mapped_capture", c.mapped_capture);
    print_counter("bytesused_zero_count", c.bytesused_zero_count);
}

}  // namespace stage06

#endif  // STAGE06_M2M_COMMON_HPP_
