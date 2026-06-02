# Stage06 Enterprise Project - V4L2 M2M Diagnostic Service

## 项目目标

把 Stage06 基础 demo 的 V4L2 M2M 知识点整理成一个工作化诊断服务：

1. CLI 参数记录测试条件。
2. StateMachine 记录 codec pipeline 状态变化。
3. Logger 输出结构化运行日志。
4. MetricsSink 输出机器可读 JSON。
5. GateEvaluator 给出客观 pass/fail。
6. Fault matrix 覆盖正常、timeout、bytesused、source change 等场景。
7. Driver-shadow 文档把用户态 counter 映射到驱动侧概念。

## 教学边界

当前项目默认模拟 V4L2 M2M queue loop，并可打开 `/dev/videoX` 执行 `QUERYCAP` 作为设备证据。它不会声称真实硬解成功。

当前实测 `/dev/video0` 为：

```text
driver=rkisp_v5, card=rkisp_mainpath, m2m_capable=no
```

这说明它是 ISP/capture 节点，不是 codec M2M 节点。默认运行仍可训练状态机、counter、report 和 gate；如果加 `--require-device`，gate 会按真实 M2M capability 要求失败。

## 文件结构

```text
enterprise_project/
├── README.md
├── build.sh
├── include/
│   ├── 00_enterprise_common.hpp
│   ├── 01_cli_config.hpp
│   ├── 02_state_machine.hpp
│   ├── 03_logger.hpp
│   ├── 04_metrics_sink.hpp
│   ├── 05_gate_evaluator.hpp
│   └── 06_m2m_diagnostic_service.hpp
├── src/
│   ├── 01_cli_config.cpp
│   ├── 02_state_machine.cpp
│   ├── 03_logger.cpp
│   ├── 04_metrics_sink.cpp
│   ├── 05_gate_evaluator.cpp
│   ├── 06_m2m_diagnostic_service.cpp
│   └── 07_enterprise_m2m_diagnostic_main.cpp
├── scripts/
│   ├── run_07_enterprise_m2m_diagnostic_service.sh
│   └── run_07_enterprise_fault_matrix.sh
├── docs/
│   ├── 07_enterprise_architecture.md
│   └── 07_enterprise_verification_guide.md
├── expected_output/
└── logs/
```

## 模块责任图

| 模块 | 文件 | 责任 | 工作映射 |
| --- | --- | --- | --- |
| CLI | `01_cli_config.*` | 参数解析和校验 | 测试条件可复现 |
| State machine | `02_state_machine.*` | 显式状态转移 | STREAMON/RECOVERY 复盘 |
| Logger | `03_logger.*` | 结构化日志 | debug report 附件 |
| Metrics | `04_metrics_sink.*` | JSON 指标输出 | 自动化回归证据 |
| Gate | `05_gate_evaluator.*` | pass/fail 规则 | bring-up 验收门禁 |
| Service | `06_m2m_diagnostic_service.*` | pipeline 主流程 | QBUF/DQBUF/fault/recovery |
| Main | `07_enterprise_m2m_diagnostic_main.cpp` | 程序入口 | 保持入口薄，便于阅读 |

## 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive/enterprise_project
./build.sh all
```

## 运行

正常路径：

```bash
./scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

故障矩阵：

```bash
./scripts/run_07_enterprise_fault_matrix.sh
```

真实 M2M capability gate：

```bash
./bin/07_enterprise_m2m_diagnostic_service \
  --device=/dev/video0 \
  --require-device \
  --output-dir=logs/run_require_m2m_gate
```

## 参数说明

| 参数 | 作用 | 可选值 | 观察点 |
| --- | --- | --- | --- |
| `--device` | 设备节点 | `/dev/videoX` | `driver/card/m2m_capable` |
| `--output-dir` | 日志输出目录 | 任意目录 | log/json 路径 |
| `--inject` | 故障注入 | `none/timeout/bytesused_zero/source_change/source_change_no_reconfigure` | verdict/failure_layer |
| `--frames` | 模拟帧数 | 正整数 | qbuf/dqbuf/decoded_frames |
| `--output-depth` | OUTPUT queue 深度 | 正整数 | max_output_depth |
| `--capture-depth` | CAPTURE queue 深度 | 正整数 | max_capture_depth |
| `--min-decoded-frames` | gate 最小解码帧数 | 非负整数 | PASS/FAIL |
| `--allowed-timeouts` | gate 允许 timeout 数 | 非负整数 | timeout 恢复是否通过 |
| `--require-device` | 要求真实 M2M 节点 | flag | 非 M2M 节点 fail |
| `--no-recover` | 关闭恢复 | flag | timeout/source-change fail |

## 预期输出

见 `expected_output/07_enterprise_m2m_diagnostic_service.txt` 和 `expected_output/07_enterprise_m2m_diagnostic_service.actual.txt`。

关键行：

```text
[INFO][querycap] driver=rkisp_v5, card=rkisp_mainpath, m2m_capable=no
[WARN][querycap] opened node is not a V4L2 M2M codec device; continue in simulated codec queue mode
[INFO][gate] ... gate_pass=yes, verdict=PASS_NORMAL_PATH, failure_layer=none
enterprise_metrics=.../enterprise_metrics.json
```

## 故障矩阵预期

```text
case                         exit_code verdict
normal                       0         PASS_NORMAL_PATH
timeout_recovered            0         PASS_WITH_RECOVERY_EVIDENCE
bytesused_zero               1         FAIL_OUTPUT_BYTESUSED_ZERO
source_change_recovered      0         PASS_WITH_RECOVERY_EVIDENCE
source_change_no_reconfigure 1         FAIL_TIMEOUT_OVER_LIMIT
```

## 生产差距

1. 还没有真实 `VIDIOC_REQBUFS/QUERYBUF/MMAP/QBUF/DQBUF`。
2. 还没有真实 Annex B 码流 payload 投喂。
3. 还没有真实 `VIDIOC_DQEVENT` 处理。
4. 还没有 CAPTURE frame dump 或 DMA-BUF export。
5. 还没有 ftrace/trace-cmd 自动采集。

这些差距是下一轮深化方向，不影响本项目作为 Stage06 工作化训练工具。
