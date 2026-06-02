# Stage03 Enterprise Project - M2M Pipeline Service (企业级补充项目)

## 项目定位

这个子工程是 Stage03 在基础 demo（01~11）之后的企业级补充项目，目标不是替代基础教学，而是把多个知识点收敛成“可运行服务骨架 + 可观察性 + 故障注入 + 验收门禁”的组合。

能力覆盖：
1. 统一 CLI 配置和可复用服务入口。
2. 状态机迁移日志（可复盘）。
3. 指标快照 JSON（可接入 CI / dashboard）。
4. 故障注入矩阵（timeout / SOURCE_CHANGE / DQBUF EAGAIN）。
5. 驱动影子线映射（S_FMT/REQBUFS/QBUF/DQBUF/STREAMON/STREAMOFF）。
6. 真实 AnnexB 输入模式（可选）与模拟模式共存。

## 目录结构

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
│   └── 06_v4l2_pipeline_service.hpp
├── src/
│   ├── 01_cli_config.cpp
│   ├── 02_logger.cpp
│   ├── 03_metrics_sink.cpp
│   ├── 04_v4l2_pipeline_service.cpp
│   └── 05_enterprise_pipeline_main.cpp
├── scripts/
│   ├── run_12_enterprise_m2m_pipeline_service.sh
│   └── run_12_enterprise_fault_matrix.sh
├── docs/
│   ├── 12_enterprise_architecture.md
│   ├── 12_enterprise_experiment_matrix.md
│   └── 12_enterprise_verification_guide.md
├── expected_output/
│   └── 12_enterprise_m2m_pipeline_service.txt
├── logs/
└── samples/
```

## 构建

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage03_v4l2_m2m_codec_foundation/enterprise_project
./build.sh
```

## 运行（正常/模拟路径）

```bash
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

## 运行（真实 AnnexB 路径）

```bash
INPUT_ANNEXB=../samples/sample_720p_h264.annexb \
MAX_INPUT_CHUNKS=8 \
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

说明：
1. `INPUT_ANNEXB` 指向 Annex B elementary stream（如 `.h264/.h265/.annexb`）。
2. `MAX_INPUT_CHUNKS=0` 表示不限制；`>0` 表示最多喂入多少个 NALU chunk。
3. 真实模式会把 `payload_chunks_total/payload_bytes_total/real_payload_mode` 写入指标。

## 运行（故障矩阵）

```bash
./scripts/run_12_enterprise_fault_matrix.sh
```

## 关键输出

1. `logs/run_xxx/enterprise_pipeline.log`：结构化运行日志。
2. `logs/run_xxx/enterprise_metrics.json`：指标快照（包含配置、计数器、通过门禁结论）。
3. 终端输出：`[enterprise] result=PASS|FAIL`。

## 知识边界（必须明确）

1. 该项目仍以“企业级工程结构与可观测性训练”为主，不等价于完整硬解画质验证。
2. 真实模式已接入真实 AnnexB payload 统计与门禁，不再是完全模拟。
3. 若底层节点不支持 `H264 -> NV12` 或权限不足，流程会在早期失败并给出 fail reason。
4. `inject_*` 选项是教学故障注入，不代表内核真实错误码分布。

## 与基础 demo 的关系

1. 01~11：强调单点概念拆解。
2. enterprise_project：强调组合能力、日志规范、实验矩阵和验收门禁。
3. 推荐顺序：先跑完 01~11，再跑本项目。
