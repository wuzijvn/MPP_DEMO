# Stage07 Enterprise Logs

企业项目运行后会在这里生成：

- `enterprise_pipeline.log`：结构化诊断日志。
- `gst_run.log`：原始 `gst-launch-1.0` 输出。
- `enterprise_metrics.json`：机器可读 metrics 与 gate 结果。
- `fault_matrix_*/summary.md`：多场景故障矩阵摘要。

日志目录不作为源码真值，重新运行脚本会生成新的时间戳目录。
