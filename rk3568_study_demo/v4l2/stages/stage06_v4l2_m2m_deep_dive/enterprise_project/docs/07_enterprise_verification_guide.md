# Enterprise Verification Guide

## 构建验证

```bash
./build.sh all
```

通过标准：生成 `bin/07_enterprise_m2m_diagnostic_service`。

## 正常路径验证

```bash
./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

通过标准：

1. exit code 为 0。
2. 输出 `enterprise_verdict=PASS_NORMAL_PATH`。
3. `logs/run_default/enterprise_metrics.json` 存在。
4. JSON 中 `gate_pass=true`。

## 故障矩阵验证

```bash
./scripts/run_07_enterprise_fault_matrix.sh
```

通过标准：

1. `normal` 通过。
2. `timeout_recovered` 通过，并带恢复证据。
3. `bytesused_zero` 失败，失败层为 `v4l2_queue_payload`。
4. `source_change_recovered` 通过。
5. `source_change_no_reconfigure` 失败，失败层为 `driver_or_hardware_completion`。

## M2M 节点 gate 验证

```bash
./bin/07_enterprise_m2m_diagnostic_service \
  --device=/dev/video0 \
  --require-device \
  --output-dir=logs/run_require_m2m_gate
```

当前环境预期：

```text
enterprise_verdict=FAIL_M2M_CAPABILITY_REQUIRED
failure_layer=device_capability
```

原因：当前 `/dev/video0` 是 `rkisp_v5/rkisp_mainpath`，不是 codec M2M。

## JSON 指标阅读

| 字段 | 含义 | 通过趋势 |
| --- | --- | --- |
| `decoded_frames` | 模拟完成帧数 | 大于等于 gate |
| `qbuf_output` | OUTPUT 投喂次数 | 随帧数增长 |
| `qbuf_capture` | CAPTURE 空帧 buffer 投喂次数 | 随帧数增长 |
| `timeout_count` | timeout 次数 | 正常为 0 |
| `source_change_count` | source change 次数 | 注入时为 1 |
| `recovery_count` | 恢复次数 | timeout/source-change 恢复时增长 |
| `m2m_capable` | 节点是否具备 codec M2M 能力 | 真实 codec 节点应为 true |
| `failure_layer` | 失败层级 | fail 时必须具体 |

## 常见验证误区

1. `PASS_NORMAL_PATH` 默认是模拟 queue loop 通过，不等于真实硬解通过。
2. `/dev/video0` 打开成功不等于 codec M2M 可用。
3. `source_change_recovered` 通过说明状态机策略正确，不说明真实驱动事件已测试。
4. `bytesused_zero` 失败是预期行为，说明 gate 能抓住用户态 payload 错误。
