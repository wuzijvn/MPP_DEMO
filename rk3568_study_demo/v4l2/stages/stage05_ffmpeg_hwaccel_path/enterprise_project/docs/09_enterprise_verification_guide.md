# 09 Enterprise Verification Guide

## 必看证据

1. `enterprise_pipeline.log`
2. `enterprise_metrics.json`
3. `summary.csv`（矩阵模式）

## 验收流程

1. 跑默认 RKMPP wrapper 路径。
2. 跑 fault matrix。
3. 需要实验 hwdevice 时，显式设置 `HW_TYPE=drm/vaapi/rkmpp` 和必要的 `DEVICE`。
4. 检查 gate 失败是否符合注入预期。
5. 记录“平台不支持导致的失败”与“代码逻辑失败”区别。

## 驱动影子线

1. 默认 RKMPP wrapper：优先确认 `h264_rkmpp/hevc_rkmpp` decoder、MPP/VPU 相关日志、软件基线对比。
2. `device create fail`：仅显式 hwdevice 模式下优先看 `/dev/dri/renderD*`、权限、驱动加载。
3. `missing hwfmt`：看 decoder backend 支持、FFmpeg 构建选项、内核能力。
4. `transfer fail`：看 hw frame 与后续格式/内存映射兼容。
