#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <string>
#include <vector>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：demo11 的命令行参数。
 * 本 demo 的知识边界：
 * 1) 只把 Annex B 码流拆成 NALU，并规划 OUTPUT QBUF 的 bytesused；
 * 2) 不打开 /dev/videoX，不执行 VIDIOC_QBUF，也不证明硬件解码成功；
 * 3) 它补的是 demo06 的占位 payload 到真实 payload 的教学桥梁。
 */
struct Args {
    std::string input;
    std::string codec = "h264";
    uint32_t max_output_bytes = 0;
    bool verbose = false;
};

/*
 * 结构体作用：描述一个 Annex B NALU 在文件中的位置。
 * 生命周期：parse_annexb_nalus() 生成，后续只读打印。
 * 关键字段：
 * - start_offset: start code 起始位置；
 * - payload_offset: NAL header 起始位置；
 * - end_offset: 当前 NALU 结束位置；
 * - start_code_len: 3 或 4 字节。
 */
struct NaluSpan {
    size_t start_offset = 0;
    size_t payload_offset = 0;
    size_t end_offset = 0;
    size_t start_code_len = 0;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 11 - bitstream payload to QBUF bytesused bridge\n", prog);
    printf("Usage:\n");
    printf("  %s --input=samples/sample.h264 --codec=h264 --max-output-bytes=0 --verbose\n",
           prog);
    printf("  %s --codec=h264 --verbose   # use built-in tiny teaching stream\n", prog);
}

/*
 * 函数作用：解析命令行参数。
 * 参数语义：
 * - input: 可选 Annex B elementary stream；为空时使用内置教学样本；
 * - codec: h264 或 h265，决定 NALU type 解析方式；
 * - max-output-bytes: 教学用 OUTPUT buffer 上限，0 表示“一整个 NALU 一次 QBUF”。
 */
bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (m2m_demo::parse_kv(argv[i], "--input", &a->input)) {
        } else if (m2m_demo::parse_kv(argv[i], "--codec", &a->codec)) {
        } else if (m2m_demo::parse_kv_u32(argv[i], "--max-output-bytes",
                                          &a->max_output_bytes)) {
        } else if (strcmp(argv[i], "--verbose") == 0) {
            a->verbose = true;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

/*
 * 函数作用：读取完整输入文件到内存。
 * 输入假设：path 指向一个 elementary stream，例如 .h264/.h265 Annex B 文件。
 * 失败现象：文件不存在或权限不足时返回 false。
 */
bool read_file(const std::string& path, std::vector<uint8_t>* out) {
    std::ifstream ifs(path.c_str(), std::ios::binary);
    if (!ifs.is_open()) {
        fprintf(stderr, "open input failed: %s\n", path.c_str());
        return false;
    }

    // 教学 demo 直接读完整文件；真实播放器/解码器通常会流式读取。
    ifs.seekg(0, std::ios::end);
    const std::streamoff size = ifs.tellg();
    if (size <= 0) {
        fprintf(stderr, "input is empty: %s\n", path.c_str());
        return false;
    }

    out->resize(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&(*out)[0]), size);
    if (!ifs.good()) {
        fprintf(stderr, "read input failed: %s\n", path.c_str());
        return false;
    }
    return true;
}

/*
 * 函数作用：追加一个教学 NALU。
 * 重要边界：
 * - 这些字节只用于训练 start code/NALU/bytesused 的关系；
 * - payload 不保证能被真实硬件解码成画面。
 */
void append_teaching_nalu(std::vector<uint8_t>* out, uint8_t header,
                          const uint8_t* payload, size_t payload_size,
                          bool four_byte_start_code) {
    if (four_byte_start_code) {
        out->push_back(0x00);
    }
    out->push_back(0x00);
    out->push_back(0x00);
    out->push_back(0x01);
    out->push_back(header);
    for (size_t i = 0; i < payload_size; ++i) {
        out->push_back(payload[i]);
    }
}

/*
 * 函数作用：生成极小 H.264 Annex B 教学样本。
 * 数据流意义：
 * - SPS/PPS/IDR/non-IDR 的 NALU type 都能被解析出来；
 * - 但它不是合格视频文件，不能作为硬件解码成功证据。
 */
std::vector<uint8_t> make_synthetic_h264_stream() {
    std::vector<uint8_t> data;
    const uint8_t sps[] = {0x42, 0x00, 0x1e, 0x8d, 0x68, 0x05, 0x01, 0xe9};
    const uint8_t pps[] = {0xce, 0x06, 0xe2};
    const uint8_t idr[] = {0x88, 0x84, 0x21, 0xa0, 0x00, 0x0f};
    const uint8_t non_idr[] = {0x9a, 0x22, 0x11, 0x00};

    append_teaching_nalu(&data, 0x67, sps, sizeof(sps), true);
    append_teaching_nalu(&data, 0x68, pps, sizeof(pps), true);
    append_teaching_nalu(&data, 0x65, idr, sizeof(idr), false);
    append_teaching_nalu(&data, 0x41, non_idr, sizeof(non_idr), false);
    return data;
}

/*
 * 函数作用：判断当前位置是否是 Annex B start code。
 * 返回值：
 * - 4: 00 00 00 01；
 * - 3: 00 00 01；
 * - 0: 不是 start code。
 */
size_t start_code_len_at(const std::vector<uint8_t>& data, size_t pos) {
    if (pos + 4 <= data.size() && data[pos] == 0x00 && data[pos + 1] == 0x00 &&
        data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
        return 4;
    }
    if (pos + 3 <= data.size() && data[pos] == 0x00 && data[pos + 1] == 0x00 &&
        data[pos + 2] == 0x01) {
        return 3;
    }
    return 0;
}

/*
 * 函数作用：扫描 Annex B start code，并生成 NALU 范围表。
 * 状态变化：
 * - 每个 NALU 的 bytesused 候选范围是 [start_offset, end_offset)。
 * 失败边界：
 * - 找不到 start code 时返回空表，说明输入可能不是 Annex B。
 */
std::vector<NaluSpan> parse_annexb_nalus(const std::vector<uint8_t>& data) {
    std::vector<NaluSpan> nalus;
    size_t pos = 0;

    while (pos < data.size()) {
        const size_t sc_len = start_code_len_at(data, pos);
        if (sc_len == 0) {
            ++pos;
            continue;
        }

        NaluSpan nalu;
        nalu.start_offset = pos;
        nalu.start_code_len = sc_len;
        nalu.payload_offset = pos + sc_len;

        size_t next = nalu.payload_offset;
        while (next < data.size() && start_code_len_at(data, next) == 0) {
            ++next;
        }
        nalu.end_offset = next;

        if (nalu.payload_offset < nalu.end_offset) {
            nalus.push_back(nalu);
        }
        pos = next;
    }
    return nalus;
}

const char* h264_type_name(uint8_t type) {
    switch (type) {
        case 1:
            return "non-IDR slice";
        case 5:
            return "IDR slice";
        case 6:
            return "SEI";
        case 7:
            return "SPS";
        case 8:
            return "PPS";
        case 9:
            return "AUD";
        default:
            return "other";
    }
}

const char* h265_type_name(uint8_t type) {
    switch (type) {
        case 32:
            return "VPS";
        case 33:
            return "SPS";
        case 34:
            return "PPS";
        case 19:
            return "IDR_W_RADL";
        case 20:
            return "IDR_N_LP";
        case 1:
            return "TRAIL_R";
        default:
            return "other";
    }
}

/*
 * 函数作用：根据 codec 解析 NALU type。
 * 输入假设：payload_offset 指向 NALU header。
 * 驱动影子线：
 * - stateful firmware 可能内部解析这些 header；
 * - stateless 路径通常需要用户态把 SPS/PPS/VPS 派生信息通过 controls 传给驱动。
 */
uint8_t parse_nalu_type(const std::vector<uint8_t>& data, const NaluSpan& nalu,
                        const std::string& codec) {
    const uint8_t header = data[nalu.payload_offset];
    if (codec == "h265" || codec == "hevc") {
        return static_cast<uint8_t>((header >> 1) & 0x3f);
    }
    return static_cast<uint8_t>(header & 0x1f);
}

const char* type_name(uint8_t type, const std::string& codec) {
    if (codec == "h265" || codec == "hevc") {
        return h265_type_name(type);
    }
    return h264_type_name(type);
}

/*
 * 函数作用：打印一个 NALU 对应的 OUTPUT QBUF 规划。
 * 关键教学点：
 * - 对 Annex B bytestream，拷进 OUTPUT buffer 的通常是 start code + NALU payload；
 * - `v4l2_buffer.bytesused` 应等于本次实际拷入的有效字节数；
 * - bytesused 过小会截断码流，过大可能把旧脏数据也交给驱动。
 */
void print_qbuf_plan_for_nalu(size_t qbuf_index, const NaluSpan& nalu,
                              uint8_t nalu_type, const std::string& codec,
                              uint32_t max_output_bytes) {
    const size_t buffer_bytes = nalu.end_offset - nalu.start_offset;
    const size_t payload_bytes = nalu.end_offset - nalu.payload_offset;

    printf("[demo11] nalu[%zu]: range=[%zu,%zu) start_code=%zu payload_bytes=%zu "
           "buffer_bytes=%zu type=%u(%s)\n",
           qbuf_index, nalu.start_offset, nalu.end_offset, nalu.start_code_len,
           payload_bytes, buffer_bytes, nalu_type, type_name(nalu_type, codec));

    if (max_output_bytes == 0 || buffer_bytes <= max_output_bytes) {
        printf("[demo11] qbuf_plan[%zu.0]: copy [%zu,%zu) -> OUTPUT buffer; "
               "set bytesused=%zu\n",
               qbuf_index, nalu.start_offset, nalu.end_offset, buffer_bytes);
        return;
    }

    size_t chunk_begin = nalu.start_offset;
    size_t chunk_id = 0;
    while (chunk_begin < nalu.end_offset) {
        size_t chunk_end = chunk_begin + max_output_bytes;
        if (chunk_end > nalu.end_offset) {
            chunk_end = nalu.end_offset;
        }
        printf("[demo11] qbuf_plan[%zu.%zu]: copy [%zu,%zu) -> OUTPUT buffer; "
               "set bytesused=%zu\n",
               qbuf_index, chunk_id, chunk_begin, chunk_end, chunk_end - chunk_begin);
        chunk_begin = chunk_end;
        ++chunk_id;
    }
    printf("[demo11] warning: split NALU only teaches bytesused chunks; real decoders may "
           "prefer access-unit aligned feeding.\n");
}

}  // namespace

/*
 * 函数作用：demo11 入口。
 * 主流程：
 * 1) 读取真实输入，或生成内置 Annex B 教学样本；
 * 2) 扫描 NALU start code；
 * 3) 打印每个 NALU 的类型、payload 范围、OUTPUT QBUF bytesused 规划；
 * 4) 输出结论边界，避免把 payload 规划误认为真实硬解证明。
 */
int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    std::vector<uint8_t> data;
    std::string source;
    if (args.input.empty()) {
        data = make_synthetic_h264_stream();
        args.codec = "h264";
        source = "built-in synthetic h264 Annex B teaching stream";
    } else {
        if (!read_file(args.input, &data)) {
            return 1;
        }
        source = args.input;
    }

    printf("[demo11] bitstream payload to QBUF bytesused bridge\n");
    printf("[demo11] source=%s\n", source.c_str());
    printf("[demo11] codec=%s input_bytes=%zu max_output_bytes=%u\n", args.codec.c_str(),
           data.size(), args.max_output_bytes);

    const std::vector<NaluSpan> nalus = parse_annexb_nalus(data);
    if (nalus.empty()) {
        printf("[demo11] FAIL: no Annex B start code found. This may be MP4/AVCC, raw slice "
               "data, or an unsupported sample.\n");
        printf("[demo11] next: convert container to elementary stream, for example "
               "`ffmpeg -i input.mp4 -c:v copy -bsf:v h264_mp4toannexb out.h264`.\n");
        return 1;
    }

    size_t total_qbuf_bytes = 0;
    for (size_t i = 0; i < nalus.size(); ++i) {
        const uint8_t nalu_type = parse_nalu_type(data, nalus[i], args.codec);
        total_qbuf_bytes += nalus[i].end_offset - nalus[i].start_offset;
        print_qbuf_plan_for_nalu(i, nalus[i], nalu_type, args.codec,
                                 args.max_output_bytes);
    }

    printf("[demo11] summary: nal_count=%zu total_qbuf_bytes=%zu\n", nalus.size(),
           total_qbuf_bytes);
    printf("[demo11] boundary: this demo does not call VIDIOC_QBUF and does not prove "
           "hardware decode. It explains what demo06's placeholder bytesused must become "
           "when real bitstream payload is used.\n");
    printf("[demo11] driver_shadow: after OUTPUT QBUF, the driver/firmware consumes these "
           "bytes, parses codec headers such as SPS/PPS/VPS, advances reference state, and "
           "later returns decoded frames through CAPTURE DQBUF.\n");
    printf("[demo11] PASS\n");
    return 0;
}
