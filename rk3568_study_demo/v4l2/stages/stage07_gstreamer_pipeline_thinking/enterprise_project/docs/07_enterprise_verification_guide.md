# Stage07 Enterprise Verification Guide

## 1. 构建

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage07_gstreamer_pipeline_thinking/enterprise_project
./build.sh all
```

通过时生成：

```text
bin/07_enterprise_gst_pipeline_service
```

## 2. 正常主路径

```bash
./scripts/run_07_enterprise_gst_pipeline_service.sh
```

检查：

```bash
grep -E '"gate_pass"|failure_layer|elapsed_ms' logs/default_run/enterprise_metrics.json
sed -n '1,120p' logs/default_run/enterprise_pipeline.log
```

预期：

```text
gate_pass=true
failure_layer=success
verdict=PASS_ENTERPRISE_GST_PIPELINE
```

## 3. caps failure

```bash
SCENARIO=caps-failure ./scripts/run_07_enterprise_gst_pipeline_service.sh
```

预期：

```text
expected_failure=true
failure_layer=link_or_caps_negotiation_failure
gate_pass=true
```

这个 case 的意义：

- 验证工具不是只会跑成功路径。
- 验证 link/caps 错误可以被分层，不会误报成 driver issue。

## 4. fault matrix

```bash
./scripts/run_07_enterprise_fault_matrix.sh
```

检查：

```bash
find logs -name summary.md | tail -1
```

每个 case 都会有：

- `stdout.log`
- `gst_run.log`
- `enterprise_pipeline.log`
- `enterprise_metrics.json`
- `status.txt`

## 5. RK 板硬件候选

如果板端有 RKMPP GStreamer/libav element：

```bash
BACKEND_ELEMENT=avdec_h264_rkmpp ./scripts/run_07_enterprise_fault_matrix.sh
```

如果没有该 element，`hardware_probe_rkmpp` case 会 fail；这是正确行为，说明当前 rootfs 没有该 GStreamer 后端候选，不能声称 GStreamer RKMPP 硬解可用。

## 6. Driver-facing 报告模板

```text
case=...
pipeline=...
backend_element=...
exit_code=...
failure_layer=...
eos_count=...
caps_mentions=...
gst_log=...
dmesg=...
device_nodes=...
what_this_proves=...
what_this_does_not_prove=...
next_action=...
```
