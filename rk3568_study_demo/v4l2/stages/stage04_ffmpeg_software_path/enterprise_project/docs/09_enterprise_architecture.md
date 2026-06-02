# 09 Enterprise Architecture

## 模块职责

1. `src/01_enterprise_pipeline_main.cpp`
- 解码服务主入口
- 状态机推进
- metrics 输出

2. `include/01_pipeline_types.hpp`
- 状态、配置、指标结构定义

## 状态机

`INIT -> INPUT_OPENED -> STREAM_READY -> DECODER_READY -> DECODING -> COMPLETED`

失败路径：
`任意阶段失败 -> FAILED`

## 驱动影子线

1. 本项目是软件路径，不直接触发 V4L2/DRM 节点。
2. 作用是作为 Stage05 硬件路径对照基线：
- 若 Stage04 正常、Stage05 异常，优先查 backend/驱动。
