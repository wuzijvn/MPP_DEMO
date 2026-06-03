# Enterprise Verification Guide

## 构建

```bash
./build.sh all
```

## VM/vim2m 正常路径

```bash
./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

通过标准：

1. exit code 为 0。
2. 输出 `enterprise_verdict=PASS_NORMAL_PATH`。
3. JSON 中 `real_ioctl_path=true`。
4. JSON 中 `mapped_output/mapped_capture/qbuf_output/qbuf_capture/dqbuf_output/dqbuf_capture` 都大于 0。

## 故障矩阵

```bash
./scripts/run_07_enterprise_fault_matrix.sh
```

通过标准：

```text
normal                       PASS_NORMAL_PATH
timeout_recovered            PASS_WITH_RECOVERY_EVIDENCE
bytesused_zero               FAIL_OUTPUT_BYTESUSED_ZERO
unsupported_format           PASS_FAULT_UNSUPPORTED_FORMAT_REJECTED
source_change_recovered      PASS_WITH_RECOVERY_EVIDENCE
source_change_no_reconfigure FAIL_TIMEOUT_OVER_LIMIT
rk_rkmpp_evidence            PASS_RK_HARDWARE_PATH_EVIDENCE
```

## RK/RKMPP 模式

```bash
MODE=rk-rkmpp ./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

有码流样本时：

```bash
MODE=rk-rkmpp INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

要求 RKMPP decoder 必须存在：

```bash
MODE=rk-rkmpp EXTRA_ARGS="--require-rkmpp" \
  ./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

## JSON 指标

| 字段 | 含义 |
| --- | --- |
| `mode` | `vm-vim2m` 或 `rk-rkmpp` |
| `real_ioctl_path` | VM 模式是否真实执行 ioctl 路径 |
| `mapped_output/mapped_capture` | mmap 是否覆盖 |
| `qbuf_output/qbuf_capture` | buffer 是否交给 driver |
| `dqbuf_output/dqbuf_capture` | buffer 是否从 driver 取回 |
| `unsupported_format_rejected` | unsupported fourcc 是否被失败或回填捕获 |
| `rk_decoder_seen` | FFmpeg 是否列出目标 RKMPP decoder |
| `rk_decode_command_ok` | 提供输入时 RKMPP 命令是否成功 |
| `failure_layer` | fail 的定位层 |

## 常见误区

1. VM `PASS_NORMAL_PATH` 说明 V4L2 M2M queue 逻辑通过，不说明 RK VPU 硬解通过。
2. RK `PASS_RK_HARDWARE_PATH_EVIDENCE` 默认只说明证据采集成功；要求硬解必须加输入和 `--require-rkmpp`。
3. `unsupported_format` 可能表现为 ioctl 失败，也可能表现为驱动回填成支持格式，两者都算捕获。
