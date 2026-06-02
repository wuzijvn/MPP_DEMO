# samples 目录说明

Stage03 当前以 ioctl 状态机教学为主，06 demo 默认用占位 bytesused 触发队列循环。
11 demo 可以读取本目录里的 Annex B elementary stream，离线展示 NALU 与 OUTPUT QBUF `bytesused` 的对应关系。

如果你有真实码流，建议放入本目录，例如：
1. `sample_720p_h264.annexb`
2. `sample_1080p_h265.annexb`

后续可扩展为：
1. 先用 `INPUT=samples/sample_720p_h264.annexb ./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh` 验证 payload 规划。
2. 从文件读取 Annex-B NALU 填入 OUTPUT queue。
3. 结合 SOURCE_CHANGE/EOS 做完整恢复路径验证。
