# 12 Enterprise Experiment Matrix

| 实验 | 场景 | 命令 | 预期 | 通过标准 | 指标关注 | 偏差解释与下一步 |
| --- | --- | --- | --- | --- | --- | --- |
| E1 正常路径（模拟） | 基础服务可跑通 | `./scripts/run_12_enterprise_m2m_pipeline_service.sh` | `result=PASS` | log+json 都生成 | `qbuf_out/dqbuf_out_ok/state_transition` | 若失败先看 fail_reason，再看驱动节点是否支持格式 |
| E2 真实 AnnexB 路径 | 真实码流进入主循环 | `INPUT_ANNEXB=../samples/sample_720p_h264.annexb MAX_INPUT_CHUNKS=8 ./scripts/run_12_enterprise_m2m_pipeline_service.sh` | `result=PASS` 或可解释 FAIL | `real_payload_mode=1` 且 `payload_bytes_total>0` | `payload_chunks_total/payload_bytes_total` | 若 FAIL 且提示 no AnnexB，先把 mp4 转 AnnexB |
| E3 timeout 注入 | 验证超时观测链路 | `INJECT_TIMEOUT=1 ./scripts/run_12_enterprise_m2m_pipeline_service.sh` | 日志出现 inject timeout | `poll_timeout > 0` | `poll_timeout` | 若为 0，检查参数是否传入；若过高，检查 loops 和注入策略 |
| E4 SOURCE_CHANGE 注入 | 验证重配路径表达 | `INJECT_SOURCE_CHANGE=1 ./scripts/run_12_enterprise_m2m_pipeline_service.sh` | 日志出现 source_change flow | `source_change >= 1` | `source_change` | 若无事件，确认注入开关和 loops 足够 |
| E5 DQBUF EAGAIN 注入 | 验证重试可观测性 | `INJECT_DQBUF_EAGAIN=1 ./scripts/run_12_enterprise_m2m_pipeline_service.sh` | 日志出现 EAGAIN | `dqbuf_eagain > 0` | `dqbuf_eagain` | 若无计数，检查注入开关；若持续失败，需区分真实错误与注入错误 |
| E6 一键矩阵 | 连续跑多类场景 | `./scripts/run_12_enterprise_fault_matrix.sh` | 多组子目录日志 | 子目录完整、每组有 metrics | 全量对比 | 若某组缺失，先看 shell set -e 导致的提前退出 |

## 指标意义

1. `state_transition`
   - 趋势：正常应覆盖主要状态迁移。
   - 异常：过低说明流程未走全。
2. `qbuf_out / qbuf_cap`
   - 趋势：应随 loops 或真实 chunk 数线性增长。
   - 异常：低于预期说明主循环提前中断。
3. `poll_timeout`
   - 趋势：正常模式应接近 0，注入模式应大于 0。
4. `source_change`
   - 趋势：仅在注入模式上升。
5. `dqbuf_eagain`
   - 趋势：仅在注入模式上升，且需能被日志解释为可恢复分支。
6. `real_payload_mode`
   - 趋势：传入 `input_annexb` 时应为 1。
7. `payload_chunks_total / payload_bytes_total`
   - 趋势：真实模式必须非 0，且与样本大小和 `max_input_chunks` 相符。
