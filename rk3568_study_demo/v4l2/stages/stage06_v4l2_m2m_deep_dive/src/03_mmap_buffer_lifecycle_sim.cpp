#include "00_stage06_m2m_common.hpp"

#include <algorithm>

namespace {

enum Owner {
    OWNER_USER,
    OWNER_DRIVER_QUEUED,
    OWNER_DRIVER_ACTIVE,
    OWNER_USER_DONE
};

std::string owner_name(Owner owner) {
    switch (owner) {
        case OWNER_USER:
            return "USER";
        case OWNER_DRIVER_QUEUED:
            return "DRIVER_QUEUED";
        case OWNER_DRIVER_ACTIVE:
            return "DRIVER_ACTIVE";
        case OWNER_USER_DONE:
            return "USER_DONE";
    }
    return "UNKNOWN";
}

struct SimBuffer {
    std::string queue;
    int index;
    size_t length;
    Owner owner;
};

void print_buffer(const SimBuffer& b) {
    std::cout << std::left << std::setw(10) << b.queue
              << " index=" << std::setw(2) << b.index
              << " length=" << std::setw(7) << b.length
              << " owner=" << owner_name(b.owner) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const int output_buffers = std::max(1, stage06::get_arg_int(argc, argv, "--output-bufs", 3));
    const int capture_buffers = std::max(1, stage06::get_arg_int(argc, argv, "--capture-bufs", 4));
    const int coded_size = std::max(1024, stage06::get_arg_int(argc, argv, "--coded-size", 262144));
    const int frame_size = std::max(1024, stage06::get_arg_int(argc, argv, "--frame-size", 1382400));

    std::vector<SimBuffer> buffers;
    for (int i = 0; i < output_buffers; ++i) {
        buffers.push_back({"OUTPUT", i, static_cast<size_t>(coded_size), OWNER_USER});
    }
    for (int i = 0; i < capture_buffers; ++i) {
        buffers.push_back({"CAPTURE", i, static_cast<size_t>(frame_size), OWNER_USER});
    }

    std::cout << "Stage06 Demo03: REQBUFS/QUERYBUF/MMAP lifecycle simulation\n";
    stage06::print_line("output_buffers", std::to_string(output_buffers));
    stage06::print_line("capture_buffers", std::to_string(capture_buffers));
    std::cout << "\n";

    std::cout << "阶段 A: VIDIOC_REQBUFS 之后，驱动/vb2 为两个队列准备 buffer 槽位\n";
    for (const SimBuffer& b : buffers) {
        print_buffer(b);
    }

    std::cout << "\n阶段 B: VIDIOC_QUERYBUF + mmap 之后，用户态获得可访问地址，但所有权仍是 USER\n";
    for (const SimBuffer& b : buffers) {
        std::cout << b.queue << "[" << b.index << "] mmap(addr=teaching_pointer, length="
                  << b.length << ") -> USER can fill/read when not queued\n";
    }

    std::cout << "\n阶段 C: QBUF 之后，所有权转移到驱动，用户态不应再写该 buffer\n";
    for (SimBuffer& b : buffers) {
        b.owner = OWNER_DRIVER_QUEUED;
        print_buffer(b);
    }

    std::cout << "\n阶段 D: STREAMON 后，驱动可以把 queued buffer 变为 active job buffer\n";
    for (SimBuffer& b : buffers) {
        if (b.index == 0) {
            b.owner = OWNER_DRIVER_ACTIVE;
        }
        print_buffer(b);
    }

    std::cout << "\n阶段 E: DQBUF 后，完成的 buffer 回到用户态；用户态读取后可再次 QBUF\n";
    for (SimBuffer& b : buffers) {
        if (b.owner == OWNER_DRIVER_ACTIVE) {
            b.owner = OWNER_USER_DONE;
        }
        print_buffer(b);
    }

    std::cout << "\n资源释放顺序：STREAMOFF -> munmap(each buffer) -> REQBUFS count=0 -> close(fd)\n";
    std::cout << "verdict=MMAP_LIFECYCLE_UNDERSTOOD\n";
    return 0;
}
