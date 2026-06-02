# Stage03 码流 payload 到 OUTPUT QBUF bytesused 桥接说明

## 这个文档补什么

Stage03 的 `06_qbuf_dqbuf_ownership` 用 16 字节占位 payload 训练队列所有权，这能证明状态机路径，但不能证明真实码流解码。

`11_bitstream_payload_to_qbuf_bytesused` 补上中间缺口：真实或教学 Annex B 码流如何被拆成 NALU、每次拷进 OUTPUT buffer 的有效范围是什么、`v4l2_buffer.bytesused` 应该填什么。

## 本 demo 不证明什么

1. 不打开 `/dev/videoX`。
2. 不执行 `VIDIOC_QBUF`。
3. 不证明硬件解码成功。
4. 不证明 fps、latency 或零拷贝。
5. 内置样本只是教学字节流，不是可播放视频文件。

## 和 demo06 的关系

| 文件 | 教学重点 | bytesused 含义 | 是否真实硬解证明 |
| --- | --- | --- | --- |
| `src/06_qbuf_dqbuf_ownership.cpp` | QBUF/DQBUF ownership loop | 当前固定 16 字节占位 payload | 否 |
| `src/11_bitstream_payload_to_qbuf_bytesused.cpp` | Annex B NALU -> OUTPUT payload 规划 | 本次实际拷入 OUTPUT buffer 的有效码流字节数 | 否 |
| 后续真实解码 demo | 文件读入 + QBUF + DQBUF CAPTURE | 实际输入帧/片/访问单元大小 | 需要真实设备和码流验证 |

## 运行命令

使用内置教学样本：

```bash
./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh
```

使用真实 H.264 Annex B elementary stream：

```bash
INPUT=samples/sample_720p_h264.annexb CODEC=h264 ./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh
```

把 MP4 中的 H.264 转成 Annex B 的示例命令：

```bash
ffmpeg -i input.mp4 -c:v copy -bsf:v h264_mp4toannexb samples/sample_720p_h264.annexb
```

## 参数说明

| 参数 | 含义 | 为什么需要 | 改动后观察什么 |
| --- | --- | --- | --- |
| `INPUT` | 可选输入 elementary stream | 训练真实 payload 规划 | NALU 数量、type、bytesused |
| `CODEC` | `h264` 或 `h265` | NALU type 解析方式不同 | SPS/PPS/VPS/IDR 名称 |
| `MAX_OUTPUT_BYTES` | 教学用 OUTPUT buffer 上限 | 模拟 buffer 容量限制 | 是否出现分块 qbuf_plan |

## 输出怎么读

1. `range=[start,end)`：这个 NALU 在输入文件中的字节范围。
2. `start_code=3/4`：Annex B 起始码长度。
3. `payload_bytes`：不含 start code 的 NALU payload 长度。
4. `buffer_bytes`：如果按 Annex B 投喂，本次可能拷入 OUTPUT buffer 的字节数。
5. `set bytesused=...`：真实 `VIDIOC_QBUF` 时应写入 `v4l2_buffer.bytesused` 的值。

## 常见误区

1. `bytesused` 不是 buffer 总容量，而是本次有效码流长度。
2. `bytesused` 太小会截断码流，可能导致 SPS/PPS 不完整或 slice 不完整。
3. `bytesused` 太大可能把旧脏数据也交给驱动。
4. MP4/AVCC 不是 Annex B，直接扫描 start code 可能失败。
5. 一个真实访问单元可能包含多个 NALU，硬件/驱动对喂入边界可能有平台约束。

## 驱动影子线

1. stateful codec 驱动/固件通常会在 OUTPUT payload 中解析 SPS/PPS/VPS、slice header、参考帧信息。
2. stateless codec 路径通常要求用户态先解析码流，再通过 controls/request API 传递参数。
3. OUTPUT QBUF 后，buffer 所有权属于驱动；用户态不能在驱动持有期间改写这段 payload。
4. 驱动消费完成后，OUTPUT DQBUF 归还输入 buffer；解码结果通过 CAPTURE DQBUF 返回。
5. 码流头不完整、bytesused 错误或边界不符合驱动预期，都可能表现为 `DQBUF` timeout、decode error 或 dmesg 中的硬件错误。

## 下一步怎么深化

下一阶段可以把 demo11 的 payload 规划接进 demo06：

1. 读取真实 `.h264/.h265` 文件。
2. 把每个访问单元拷贝到 OUTPUT mmap buffer。
3. 用真实 `bytesused` 执行 `VIDIOC_QBUF`。
4. 从 CAPTURE `DQBUF` 读取 decoded frame。
5. 对比 dmesg、`bytesused`、`sequence`、timeout 次数，判断是否真正在解码。
