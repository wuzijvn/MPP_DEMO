# Stage06 Start Here

<<<<<<< HEAD
## 0. 本阶段怎么分环境

| 环境 | 推荐动作 | 通过标准 |
| --- | --- | --- |
| VM | 跑基础 demo、enterprise diagnostic service、fault matrix | 能解释 V4L2 M2M 状态机和 timeout/source-change 报告 |
| RK 板 | 先跑 `run_00_rk_board_reality_check.sh` | 能证明是否存在 M2M codec 节点；没有则切回 Stage05 RKMPP |

不要把 VM 的虚拟 M2M 结果写成 RK 板硬解证据，也不要把 RK 板没有 V4L2 M2M codec 节点写成“V4L2 M2M 学习失败”。

## RK 板端先跑
=======
## 先确认你在哪条线
>>>>>>> 57cb4fd39a36343ee19a989d109b951a768d9a52

| 环境 | 入口 | 验证目标 |
| --- | --- | --- |
| VM | `./scripts/run_all_stage06.sh` | 用 `vim2m` 真实跑 V4L2 M2M queue 逻辑 |
| RK 板 | `./scripts/run_06_rk_board_rkmpp_hardware_path.sh` | 收集 RKMPP/FFmpeg/板端硬件路径证据 |

不要把两条线混在一起：VM 的 `vim2m` 不等于 codec 硬解，RK 板的 ISP/camera 节点也不等于 codec M2M。

## 阅读顺序

1. `README.md`：看 stage03 到 stage06 深化点。
2. `src/01_vm_vim2m_device_discovery.cpp`：真实 `open/QUERYCAP/ENUM_FMT`。
3. `src/02_vm_vim2m_format_negotiation.cpp`：真实 `TRY_FMT/S_FMT` 和驱动回填。
4. `src/03_vm_vim2m_mmap_lifecycle.cpp`：真实 `REQBUFS/QUERYBUF/MMAP/munmap`。
5. `src/04_vm_vim2m_queue_loop.cpp`：真实 `QBUF/STREAMON/poll/DQBUF/requeue`。
6. `src/05_vm_vim2m_fault_injection.cpp`：真实故障注入，不只打印流程。
7. `src/06_rk_board_rkmpp_hardware_path.cpp`：RK 板独立硬件证据路径。
8. `enterprise_project/README.md`：带 state machine、metrics、gate 的工作化版本。
9. `docs/06_post_code_debugging_guide.md`：代码跑通后的手动调试教程，练 CPU、耗时、内存、fd/mmap、queue 和 RK 证据定位。

## 当前 VM 期待

```text
driver=vim2m
m2m_capable=yes
streaming_capable=yes
```

<<<<<<< HEAD
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

## 报告环境头模板

```text
environment=VM|RK_BOARD
backend=v4l2_m2m_virtual|simulation|rkmpp|no_v4l2_m2m_codec_node
hardware_proof=yes|no
what_this_proves=...
what_this_does_not_prove=...
```
=======
Demo04 看到 `DQBUF VIDEO_CAPTURE` 和 `DQBUF VIDEO_OUTPUT` 后，才算真正超过 stage03 的 capability 探测。
>>>>>>> 57cb4fd39a36343ee19a989d109b951a768d9a52
