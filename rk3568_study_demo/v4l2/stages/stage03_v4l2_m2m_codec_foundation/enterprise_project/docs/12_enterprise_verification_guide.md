# 12 Enterprise Verification Guide

## 1. 最小验证步骤（模拟）

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage03_v4l2_m2m_codec_foundation/enterprise_project
./build.sh
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

检查：
1. 终端有 `result=PASS|FAIL`。
2. `logs/run_xxx/enterprise_pipeline.log` 存在。
3. `logs/run_xxx/enterprise_metrics.json` 存在且字段完整。

## 2. 真实 AnnexB 验证

```bash
INPUT_ANNEXB=../samples/sample_720p_h264.annexb \
MAX_INPUT_CHUNKS=8 \
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

检查：
1. 日志包含 `real_payload_mode=1`。
2. metrics 中 `payload_chunks_total > 0` 且 `payload_bytes_total > 0`。
3. gate 通过时应输出 `result=PASS`。

若你手里是 `mp4`，先转 AnnexB：

```bash
ffmpeg -i input.mp4 -c:v copy -bsf:v h264_mp4toannexb out.h264
```

## 3. 故障注入验证

```bash
./scripts/run_12_enterprise_fault_matrix.sh
```

检查：
1. `fault_matrix_xxx/normal`、`real_annexb`、`inject_timeout`、`inject_source_change`、`inject_dqbuf_eagain` 目录齐全。
2. 每组都至少包含 `console.log`、`enterprise_pipeline.log`、`enterprise_metrics.json`。

## 4. 失败分层定位模板

1. 命令层：脚本参数是否传入（尤其 `INPUT_ANNEXB`）。
2. 节点层：`/dev/videoX` 是否存在、权限是否满足。
3. 输入层：AnnexB 是否有 start code（`00 00 01` / `00 00 00 01`）。
4. 协商层：`S_FMT` 的 fourcc/尺寸是否被驱动支持。
5. 队列层：`REQBUFS` 是否成功并返回有效 count。
6. 状态机层：是否进入 STREAMING 后又异常退出。

## 5. 与 Stage03 基础 demo 的联动

1. 若 enterprise 项目在 `S_FMT` 失败，先回到 `run_04_try_set_format.sh` 缩小问题。
2. 若在 buffer 阶段失败，先回到 `run_05_request_query_mmap.sh`。
3. 若在循环/超时定位困难，先回到 `run_06_qbuf_dqbuf_ownership.sh` 和 `run_08_poll_timeout.sh`。
4. 若真实输入解析失败，先回到 `run_11_bitstream_payload_to_qbuf_bytesused.sh` 验证 AnnexB 结构。
