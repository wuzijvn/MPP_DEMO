# 09 Enterprise Experiment Matrix

| Case | 命令附加参数 | 预期结果 | 关注指标 |
| --- | --- | --- | --- |
| normal | 无 | RK3568 默认 `h264_rkmpp` wrapper 成功出帧则 PASS | frame_recv/frame_cpu_visible/fallback_count |
| explicit_hwdevice | `HW_TYPE=drm DEVICE=/dev/dri/renderD128` | 仅用于显式 hwdevice 实验 | frame_hw/frame_cpu_visible/transfer_ok |
| inject_device_fail | `HW_TYPE=drm --inject-device-create-fail` | FAIL 或 fallback 证据明确 | fallback_count 增长 |
| inject_force_sw | `--inject-force-sw-fallback` | FAIL（no hardware frame） | frame_hw=0 frame_cpu_visible>0 |
| inject_transfer_fail | `HW_TYPE=drm --inject-transfer-fail` | FAIL（transfer 证据不足） | hw_transfer_fail 增长 |
| inject_missing_hwfmt | `HW_TYPE=drm --inject-missing-hwfmt` | FAIL 或 fallback | fallback_count 增长 |

运行：

```bash
./scripts/run_09_enterprise_fault_matrix.sh
```

输出：
- `logs/fault_matrix_xxx/summary.csv`
- 每个 case 下的 `enterprise_pipeline.log` 与 `enterprise_metrics.json`
