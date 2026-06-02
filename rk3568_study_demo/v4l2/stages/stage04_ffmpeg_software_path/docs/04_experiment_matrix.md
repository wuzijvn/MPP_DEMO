# Stage04 实验矩阵

| 实验 | 目标 | 命令 | 观察指标 | 通过标准 |
| --- | --- | --- | --- | --- |
| E1 基础探测 | 输入与流识别 | `run_01` | 流数量、视频流索引 | 有效视频流 |
| E2 demux 可视化 | packet 观察 | `run_02` | packet 数、size、pts | 输出稳定无异常退出 |
| E3 decode 主链路 | packet->frame | `run_03` | frame 数、分辨率 | frame 连续输出 |
| E4 所有权验证 | 生命周期 | `run_04` | packet/frame 释放日志 | 无泄漏迹象 |
| E5 时基理解 | 时间戳换算 | `run_05` | pts/dts 秒级值 | 数值连续可解释 |
| E6 故障注入 | cleanup 路径 | `run_07 INJECT_STEP=3` | cleanup 日志 | FAIL 但释放完整 |
| E7 基线性能 | 软件 fps | `run_08` | fps | 有可复现实测值 |
| E8 企业级验证 | 结构化交付 | `run_09` | metrics json/log | gate pass |
