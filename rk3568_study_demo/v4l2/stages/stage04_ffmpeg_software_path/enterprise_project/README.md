# Stage04 Enterprise Project - FFmpeg Software Pipeline Service

## 项目目标

把 Stage04 基础 demo 收敛为一个“企业级可运行骨架”：
1. 统一 CLI
2. 状态机日志
3. 指标 JSON
4. 故障注入
5. 门禁结果

## 结构

```text
enterprise_project/
├── README.md
├── build.sh
├── include/
│   ├── 00_enterprise_common.hpp
│   └── 01_pipeline_types.hpp
├── src/
│   └── 01_enterprise_pipeline_main.cpp
├── scripts/
│   ├── run_09_enterprise_ffmpeg_pipeline_service.sh
│   └── run_09_enterprise_fault_matrix.sh
├── docs/
│   ├── 09_enterprise_architecture.md
│   ├── 09_enterprise_experiment_matrix.md
│   └── 09_enterprise_verification_guide.md
├── expected_output/
│   └── 09_enterprise_ffmpeg_pipeline_service.txt
└── logs/
```

## 构建

```bash
./build.sh
```

## 运行

```bash
./scripts/run_09_enterprise_ffmpeg_pipeline_service.sh
./scripts/run_09_enterprise_fault_matrix.sh
```

## 边界

1. 当前仍是软件解码企业级骨架，不是硬件路径证明。
2. 若缺少 FFmpeg 开发库，构建会失败并退出。
