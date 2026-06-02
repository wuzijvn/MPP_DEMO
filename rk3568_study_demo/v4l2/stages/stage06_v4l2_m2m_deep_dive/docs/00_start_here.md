# Stage06 Start Here

## RK 板端先跑

```bash
./scripts/run_00_rk_board_reality_check.sh
```

如果没有 V4L2 M2M codec 节点，这是正常板端约束；继续阅读 `docs/05_rk_board_no_m2m_adaptation.md`，再跑 Stage06 模拟 demo。

## 阅读顺序

1. `README.md`：先看阶段边界和命令。
2. `src/01_decoder_ioctl_sequence_map.cpp`：建立 stateful decoder 全流程地图。
3. `src/02_format_negotiation_probe.cpp`：理解 `TRY_FMT/S_FMT` 和设备 capability。
4. `src/03_mmap_buffer_lifecycle_sim.cpp`：理解 `REQBUFS/QUERYBUF/MMAP` 生命周期。
5. `src/04_qbuf_dqbuf_poll_timeout_sim.cpp`：理解 QBUF/DQBUF 与 timeout。
6. `src/05_source_change_eos_drain_sim.cpp`：理解 source change、EOS、drain。
7. `src/06_timeout_debug_report_template.cpp`：学习怎么写 debug report。
8. `docs/05_rk_board_no_m2m_adaptation.md`：理解 RKMPP 真实路径和 V4L2 M2M 概念路径的分工。
9. `enterprise_project/README.md`：进入工作化诊断工具。

## 文件责任图

| 文件 | 责任 | 先看原因 |
| --- | --- | --- |
| `include/00_stage06_m2m_common.hpp` | 基础工具函数 | 认识 xioctl/fourcc/参数解析 |
| `src/01_decoder_ioctl_sequence_map.cpp` | 主流程地图 | 建立完整 ioctl 顺序 |
| `src/02_format_negotiation_probe.cpp` | 格式协商探测 | 区分真实节点和模拟报告 |
| `src/03_mmap_buffer_lifecycle_sim.cpp` | mmap buffer 生命周期 | 理解 USER/DRIVER 所有权 |
| `src/04_qbuf_dqbuf_poll_timeout_sim.cpp` | queue loop 和 timeout | 训练 DQBUF timeout 归因 |
| `src/05_source_change_eos_drain_sim.cpp` | source change/EOS/drain | 训练状态机边界 |
| `src/06_timeout_debug_report_template.cpp` | debug report 生成 | 学会对外沟通证据 |
| `enterprise_project/src/06_m2m_diagnostic_service.cpp` | 企业项目核心服务 | 看状态机、counter、故障注入 |

## 当前环境提醒

当前运行中 `/dev/video0` 可以打开，但 `QUERYCAP` 显示 `driver=rkisp_v5, card=rkisp_mainpath, m2m_capable=no`。这说明它是 ISP/capture 节点，不是 codec M2M 节点。Stage06 默认仍可用模拟模式训练队列和报告能力；要验证真实 VPU codec，需要找到具备 `V4L2_CAP_VIDEO_M2M` 或 `V4L2_CAP_VIDEO_M2M_MPLANE` 的节点。
