# 12 Enterprise Architecture

## 1. 模块职责

1. `src/01_cli_config.cpp`
   - 参数解析与校验。
   - 统一把 shell 环境映射到 `PipelineConfig`。
2. `src/02_logger.cpp`
   - 双写日志（stdout + 文件），保证可读与可追溯。
3. `src/03_metrics_sink.cpp`
   - 输出稳定 JSON schema，便于自动化消费。
4. `src/04_v4l2_pipeline_service.cpp`
   - 负责 V4L2 关键调用流程与故障注入触发点。
   - 新增真实 AnnexB 输入解析与 payload chunk 规划。
5. `src/05_enterprise_pipeline_main.cpp`
   - 状态机推进、门禁判定、最终结果汇总。

## 2. 状态机

`INIT -> DEVICE_OPENED -> CAPS_QUERIED -> FORMATS_SET -> BUFFERS_REQUESTED -> STREAMING -> DRAINING -> STOPPED`

失败路径：
`任意阶段失败 -> FAILED`

## 3. 驱动影子线映射

1. `VIDIOC_S_FMT`
   - 影子线：驱动侧格式协商与 sizeimage/stride 回填。
2. `VIDIOC_REQBUFS`
   - 影子线：vb2 queue_setup + buffer metadata 建立。
3. `QBUF/DQBUF`（当前以计数器和注入方式训练）
   - 影子线：ownership 在用户态和驱动态之间往返。
4. `VIDIOC_STREAMON/OFF`
   - 影子线：硬件管线状态切换。
5. SOURCE_CHANGE 注入
   - 影子线：CAPTURE 需要 reconfigure 契约。

## 4. 可观测性设计

1. 结构化日志：事件时间、等级、动作。
2. 指标 JSON：配置 + 计数器 + 通过门禁。
3. 故障矩阵脚本：可复现实验。
4. 真实模式专属指标：`real_payload_mode`、`payload_chunks_total`、`payload_bytes_total`。

## 5. 模式选择

1. 模拟模式（默认）
   - 不传 `input_annexb`。
   - 使用 `output_bytesused` 作为每轮教学 payload。
2. 真实模式（可选）
   - 传 `--input-annexb=...`。
   - 从 AnnexB 文件切 NALU chunk，驱动主循环以真实 payload 统计推进。

## 6. 与真实生产差距

1. 还未接入 CAPTURE 帧内容校验（例如 YUV hash/CRC）。
2. 还未实现 mmap buffer 全生命周期明细（本阶段基础 demo 已覆盖，该项目先强调集成框架）。
3. 还未与系统服务化（systemd/healthcheck/告警）对接。
