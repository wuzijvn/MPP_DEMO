# Stage06 Experiment Matrix

| 实验 | 知识点 | 真实场景 | 命令 | 预期输出 | 通过标准 | 指标 | 假信号 | 失败层 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 基础全跑 | Stage06 全链路 | 学习包自检 | `./scripts/run_all_stage06.sh` | 6 个 PASS | exit 0 | 每个 demo log | 模拟通过不等于真实硬解 | 测试覆盖层 | 看 logs 目录 |
| ioctl 顺序 | stateful decoder | 解释 OUTPUT/CAPTURE | `./scripts/run_01_decoder_ioctl_sequence_map.sh` | `SEQUENCE_MAP_READY` | 有 source_change/eos counter | qbuf/dqbuf count | 只模拟不访问设备 | 概念层 | 进入 demo02 |
| 格式协商 | TRY_FMT/S_FMT | 格式不支持 | `SIMULATE=1 ./scripts/run_02_format_negotiation_probe.sh` | simulated report | fourcc/size 清楚 | output/capture fourcc | 模拟不证明驱动支持 | 格式层 | 换真实 M2M 节点 |
| buffer 生命周期 | REQBUFS/MMAP | buffer 泄漏/悬挂 | `./scripts/run_03_mmap_buffer_lifecycle_sim.sh` | owner 状态变化 | 释放顺序正确 | buffer count | 模拟地址不是真实 mmap | buffer 层 | 接真实 QUERYBUF |
| timeout 恢复 | poll/DQBUF | 长跑卡住 | `./scripts/run_04_qbuf_dqbuf_poll_timeout_sim.sh` | `TIMEOUT_DETECTED_AND_RECOVERED` | recovery_count=1 | timeout_count | 恢复策略简化 | 驱动/硬件完成层 | 查 dmesg/trace |
| source change | CAPTURE 重配 | 分辨率变化 | `./scripts/run_05_source_change_eos_drain_sim.sh` | `SOURCE_CHANGE_EOS_DRAIN_HANDLED` | source_change=1 | recovery/eos | 没有真实 event fd | 状态机层 | 加 `VIDIOC_DQEVENT` |
| debug report | 报告能力 | 给驱动同学提 issue | `./scripts/run_06_timeout_debug_report_template.sh` | markdown report | report 文件存在 | timeout/source flags | 模板不等于根因 | 文档层 | 填真实命令日志 |
| 企业正常 | gate | bring-up 快速验证 | `./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh` | `PASS_NORMAL_PATH` | gate_pass=yes | decoded/qbuf/dqbuf | 默认可模拟 | 服务层 | 加 require-device |
| 企业故障矩阵 | fault injection | 回归测试 | `./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh` | summary.tsv | 正常/恢复 pass，故障 fail | verdict/failure_layer | fail 是预期 | gate 层 | 看每个 metrics JSON |
| M2M capability gate | 节点分类 | video 节点混淆 | `./enterprise_project/bin/07_enterprise_m2m_diagnostic_service --require-device --output-dir=enterprise_project/logs/run_require_m2m_gate` | `FAIL_M2M_CAPABILITY_REQUIRED` | 当前 ISP 节点应 fail | m2m_capable | 有 video 节点不等于 codec | device_capability | 找 M2M 节点 |

## 指标解释

| 指标 | 期望趋势 | 偏差含义 | 可能层级 | 下一步验证 |
| --- | --- | --- | --- | --- |
| `qbuf_output` | 随帧数增长 | 没增长说明没有投喂码流 | 用户态/队列 | 打印 buffer index/bytesused |
| `qbuf_capture` | 至少保持足够空帧 buffer | 太少会背压硬件输出 | buffer/性能 | 调整 CAPTURE buffer count |
| `dqbuf_capture` | 随 decoded frame 增长 | 不增长说明没完成输出 | bitstream/driver/hardware | dmesg/trace/poll |
| `timeout_count` | 正常为 0 | job 未完成或状态机错误 | 驱动/硬件/PM | IRQ、firmware、runtime PM |
| `source_change_count` | 只在分辨率变化出现 | 未处理会卡住 | 状态机/driver event | DQEVENT + CAPTURE 重配 |
| `m2m_capable` | codec 节点为 yes | no 说明不是 codec M2M | 设备节点 | v4l2-ctl list-devices |
| `failure_layer` | fail 时必须具体 | `none` 但失败说明 gate 漏洞 | gate 逻辑 | 修正 evaluator |
