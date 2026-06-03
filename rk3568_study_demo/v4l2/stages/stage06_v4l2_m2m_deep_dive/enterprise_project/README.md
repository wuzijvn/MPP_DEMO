# Stage06 Enterprise Project - Dual-Mode M2M Diagnostic Service

这个项目把 Stage06 的基础 demo 做成工作化诊断服务：有 CLI、状态机、结构化日志、JSON metrics、gate 和 fault matrix。

它现在有两个模式：

| 模式 | 命令 | 目标 |
| --- | --- | --- |
| `vm-vim2m` | 默认 | 用 VM `/dev/video0` `vim2m` 真实执行 V4L2 M2M ioctl、mmap、QBUF/DQBUF/poll |
| `rk-rkmpp` | `MODE=rk-rkmpp` | 在 RK 板收集 FFmpeg/RKMPP、设备节点和 dmesg 硬件路径证据 |

## 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive/enterprise_project
./build.sh all
```

## VM 正常路径

```bash
./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

预期：

```text
enterprise_verdict=PASS_NORMAL_PATH
real_ioctl_path=true
qbuf_output/qbuf_capture/dqbuf_output/dqbuf_capture > 0
```

## 故障矩阵

```bash
./scripts/run_07_enterprise_fault_matrix.sh
```

预期：

```text
normal                       PASS_NORMAL_PATH
timeout_recovered            PASS_WITH_RECOVERY_EVIDENCE
bytesused_zero               FAIL_OUTPUT_BYTESUSED_ZERO
unsupported_format           PASS_FAULT_UNSUPPORTED_FORMAT_REJECTED
source_change_recovered      PASS_WITH_RECOVERY_EVIDENCE
source_change_no_reconfigure FAIL_TIMEOUT_OVER_LIMIT
rk_rkmpp_evidence            PASS_RK_HARDWARE_PATH_EVIDENCE
```

## RK 模式

```bash
MODE=rk-rkmpp ./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

有码流样本时：

```bash
MODE=rk-rkmpp INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

要求 decoder 必须存在：

```bash
MODE=rk-rkmpp EXTRA_ARGS="--require-rkmpp" \
  ./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

## 关键模块

| 模块 | 文件 | 责任 |
| --- | --- | --- |
| CLI | `src/01_cli_config.cpp` | 解析 `--mode/--device/--inject/--input/--decoder` |
| State machine | `src/02_state_machine.cpp` | 记录 pipeline 状态转移 |
| Logger | `src/03_logger.cpp` | 写 `enterprise_pipeline.log` |
| Metrics | `src/04_metrics_sink.cpp` | 写 `enterprise_metrics.json` |
| Gate | `src/05_gate_evaluator.cpp` | 区分 VM queue gate 和 RK evidence gate |
| Service | `src/06_m2m_diagnostic_service.cpp` | 双模式主流程 |

## 上线后调试

企业服务跑通后，继续按 `../docs/06_post_code_debugging_guide.md` 手动跟练 CPU、耗时、内存、fd/mmap 生命周期、queue counters、software fallback 和 RK driver evidence。这里故意不提供一键脚本，因为工作中需要你能读懂每个指标代表哪一层的问题。

## 边界

VM `vim2m` 证明 V4L2 M2M 队列逻辑，不证明 RK VPU codec 硬解。RK 模式保留硬件路径证据入口，但不发明 RKMPP SDK API，也不把 ISP/camera 节点当 codec M2M。
