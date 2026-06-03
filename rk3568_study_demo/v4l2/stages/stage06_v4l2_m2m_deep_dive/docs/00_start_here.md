# Stage06 Start Here

## 先确认你在哪条线

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

Demo04 看到 `DQBUF VIDEO_CAPTURE` 和 `DQBUF VIDEO_OUTPUT` 后，才算真正超过 stage03 的 capability 探测。
