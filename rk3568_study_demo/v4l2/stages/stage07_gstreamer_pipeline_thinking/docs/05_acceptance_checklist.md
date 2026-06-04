# Stage07 Acceptance Checklist

## 基础能力

- [ ] 能解释 element、pad、caps、caps negotiation。
- [ ] 能说明 capsfilter 和 videoconvert 的区别。
- [ ] 能解释 queue 的线程边界、背压和 latency tradeoff。
- [ ] 能用 `gst-inspect-1.0` 看 element 是否存在和 pad template。
- [ ] 能用 `GST_DEBUG` 采集 caps/pad/pipeline 日志。
- [ ] 能把 link failure 和 runtime backend failure 分开。

## 代码和脚本

- [ ] `./build.sh all-with-enterprise` 成功。
- [ ] `./scripts/run_all_stage07.sh` 成功生成日志目录。
- [ ] `./enterprise_project/scripts/run_07_enterprise_gst_pipeline_service.sh` 生成 JSON metrics。
- [ ] `./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh` 生成 `summary.md`。

## 报告能力

- [ ] 报告中包含完整 pipeline。
- [ ] 报告中包含 element/backend 选择。
- [ ] 报告中包含 exit code、EOS/error、caps/failure_layer。
- [ ] 报告中明确 what this proves / what this does not prove。
- [ ] 硬件路径报告不把 plugin installed 当作 VPU proof。

## 进入下一阶段的条件

你可以进入 Stage08，当你能独立回答：

1. 为什么 `videotestsrc ! video/x-h264 ! fakesink` 是 link/caps 错误？
2. 为什么 `avdec_h264_rkmpp` installed 不等于硬解已验证？
3. queue 深度如何影响 latency 和背压？
4. FFmpeg 硬解成功但 GStreamer 失败时，你会优先比较哪些条件？
5. `GST_DEBUG` 和 dmesg 分别证明什么？
