# Stage06 Experiment Matrix

| 实验 | 命令 | 预期 verdict | 证明点 |
| --- | --- | --- | --- |
| VM 全跑 | `./scripts/run_all_stage06.sh` | 6 个 PASS | 基础 demo + 企业 VM 服务都能跑 |
| 设备发现 | `./scripts/run_01_vm_vim2m_device_discovery.sh` | `PASS_VM_M2M_DEVICE_DISCOVERY` | `QUERYCAP/ENUM_FMT` 真实执行 |
| 格式协商 | `./scripts/run_02_vm_vim2m_format_negotiation.sh` | `PASS_VM_FORMAT_NEGOTIATION` | `TRY_FMT/S_FMT` 和驱动回填 |
| mmap 生命周期 | `./scripts/run_03_vm_vim2m_mmap_lifecycle.sh` | `PASS_VM_MMAP_LIFECYCLE` | `REQBUFS/QUERYBUF/MMAP/munmap` |
| queue loop | `./scripts/run_04_vm_vim2m_queue_loop.sh` | `PASS_VM_REAL_QUEUE_LOOP` | `QBUF/poll/DQBUF/requeue` |
| VM 故障 | `./scripts/run_05_vm_vim2m_fault_injection.sh` | `PASS_FAULT_*` | 故障真实打到 driver |
| RK 证据 | `./scripts/run_06_rk_board_rkmpp_hardware_path.sh` | `PASS_RK_HARDWARE_PATH_EVIDENCE_COLLECTED` | RKMPP/FFmpeg/板端证据 |
| 企业 VM | `./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh` | `PASS_NORMAL_PATH` | metrics/gate 证明真实 ioctl path |
| 企业矩阵 | `./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh` | summary.tsv | 正常、恢复、故障、RK 证据分流 |
| 上线后调试跟练 | 手动阅读并执行 `docs/06_post_code_debugging_guide.md` 中的命令 | 能写出 debug report | CPU、耗时、内存、fd/mmap、queue、fallback、RK driver evidence |

## 企业矩阵预期

```text
normal                       0  PASS_NORMAL_PATH
timeout_recovered            0  PASS_WITH_RECOVERY_EVIDENCE
bytesused_zero               1  FAIL_OUTPUT_BYTESUSED_ZERO
unsupported_format           0  PASS_FAULT_UNSUPPORTED_FORMAT_REJECTED
source_change_recovered      0  PASS_WITH_RECOVERY_EVIDENCE
source_change_no_reconfigure 1  FAIL_TIMEOUT_OVER_LIMIT
rk_rkmpp_evidence            0  PASS_RK_HARDWARE_PATH_EVIDENCE
```

## 指标解释

| 指标 | 期望 | 含义 |
| --- | --- | --- |
| `real_ioctl_path` | VM 模式必须 `true` | 不允许用模拟代替 V4L2 M2M 验证 |
| `mapped_output/mapped_capture` | 大于 0 | mmap 生命周期被覆盖 |
| `qbuf_output/qbuf_capture` | 大于 0 | buffer 交给 driver |
| `dqbuf_output/dqbuf_capture` | 大于 0 | driver 把 buffer 交回 user |
| `decoded_frames` | 达到 gate | 对 vim2m 表示完成帧数，不表示 codec 硬解帧 |
| `failure_layer` | fail 时具体 | 能定位到 device、format、queue、gate、RK backend |

## 手动调试指标

代码跑通后，还要能手动收集这些信号：

| 指标 | 推荐入口 | 证明点 |
| --- | --- | --- |
| CPU/thread 热点 | `top -H`、`pidstat`、`perf` | 是否忙等、copy 热点或 syscall 过密 |
| 耗时/latency | `/usr/bin/time -v`、`strace -T` | 是否卡在 `poll/ioctl/DQBUF` |
| 内存增长 | `ps`、`pmap`、`smaps_rollup`、Valgrind/ASan | 是否有 heap 或 mmap 泄漏 |
| fd 生命周期 | `/proc/<pid>/fd`、`lsof`、`strace openat/close` | `open/close` 是否对称 |
| mmap 生命周期 | `strace mmap/munmap` | `mmap/munmap` 是否对称 |
| RK 硬件证据 | FFmpeg decoder、dmesg、thermal/sysfs | 是否真的进入 RK 硬件路径 |
