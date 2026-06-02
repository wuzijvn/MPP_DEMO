# Stage05 Enterprise Project - FFmpeg HWAccel Pipeline Service（深耕版）

## 项目目标

把 Stage05 基础 demo 收敛成“可工程交付”的企业级补充项目，核心强调：
1. 多模块职责拆分（非单文件）：CLI / 状态机 / 日志 / 指标 / gate / service。
2. 硬件路径真实性门禁：默认以 `h264_rkmpp` decoder wrapper + 成功出帧作为 RK3568 硬解证据。
3. 故障注入矩阵：强制 fallback；显式 `HW_TYPE=...` 模式下可进一步覆盖设备创建失败、transfer 失败、缺失 hwfmt。
4. 可复盘证据：`enterprise_pipeline.log + enterprise_metrics.json + summary.csv`。
5. 计数语义防误读：`cpu_visible` 仅表示“CPU 可见输出帧”，不等价于“软件解码”。

## 文件结构

```text
enterprise_project/
├── README.md
├── build.sh
├── include/
│   ├── 00_enterprise_common.hpp
│   ├── 01_pipeline_types.hpp
│   ├── 02_cli_config.hpp
│   ├── 03_state_machine.hpp
│   ├── 04_logger.hpp
│   ├── 05_metrics_sink.hpp
│   ├── 06_hwaccel_pipeline_service.hpp
│   └── 07_gate_evaluator.hpp
├── src/
│   ├── 01_cli_config.cpp
│   ├── 02_state_machine.cpp
│   ├── 03_logger.cpp
│   ├── 04_metrics_sink.cpp
│   ├── 05_gate_evaluator.cpp
│   ├── 06_hwaccel_pipeline_service.cpp
│   └── 07_enterprise_pipeline_main.cpp
├── scripts/
│   ├── run_09_enterprise_ffmpeg_hwaccel_service.sh
│   └── run_09_enterprise_fault_matrix.sh
├── docs/
│   ├── 09_enterprise_architecture.md
│   ├── 09_enterprise_experiment_matrix.md
│   ├── 09_enterprise_verification_guide.md
│   └── 10_enterprise_project_walkthrough.md
├── expected_output/
│   └── 09_enterprise_ffmpeg_hwaccel_service.txt
└── logs/
```

## 构建

```bash
./build.sh
```

## 运行（正常路径）

```bash
./scripts/run_09_enterprise_ffmpeg_hwaccel_service.sh
```

默认命令不传 `--hw-type`，等价于：

```bash
DECODER=h264_rkmpp ./scripts/run_09_enterprise_ffmpeg_hwaccel_service.sh
```

只有在明确要实验某个 hwdevice 后端时才设置：

```bash
HW_TYPE=drm DEVICE=/dev/dri/renderD128 ./scripts/run_09_enterprise_ffmpeg_hwaccel_service.sh
```

## 运行（故障矩阵）

```bash
./scripts/run_09_enterprise_fault_matrix.sh
```

## 深耕点（相比 stage04）

1. 代码深度：由单文件扩展为 7 个源文件 + 8 个头文件。
2. 行为深度：包含真实 packet/frame 解码循环，不是纯日志模拟。
3. 验收深度：`gate` 区分 RKMPP wrapper 证据和显式 hwdevice/hwframe 证据，通过/失败可自动化。
4. 诊断深度：故障注入覆盖 4 类高频问题，输出可比较矩阵。
5. 命名深度：把容易误解的 `sw` 计数升级为 `cpu_visible`，减少“把内存形态误判成软解”的风险。

## 边界

1. 本项目仍属于“学习型企业骨架”，不是生产级播放器/转码系统。
2. 不承诺所有平台都能命中显式 hwdevice/hwframe 路径；RK3568 当前默认以 `h264_rkmpp` wrapper 作为主线。
