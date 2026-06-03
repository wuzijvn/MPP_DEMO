# Stage05 - FFmpeg Hardware Acceleration Path（完整教学包）

## 当前环境定位

Stage05 是 `RK 板优先` 的真实硬件路径验证阶段。

1. RK 板：默认使用 FFmpeg `h264_rkmpp` / `hevc_rkmpp` 验证真实硬解路径，并用 decoder selection、frame count、software-vs-RKMPP benchmark、dmesg/日志作为证据。
2. VM：可以学习 FFmpeg hwaccel 的 API 和后端概念，也可以做 VAAPI/V4L2 M2M 框架对照；但 VM 上没有 RKMPP 时，不要求 `h264_rkmpp` 成功。
3. Stage05 不证明 RK 板存在通用 V4L2 M2M codec 节点；这件事由 Stage06 `run_00_rk_board_reality_check.sh` 判断。

完整双环境路线见：

```bash
less /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_dual_environment_codec_route.md
```

## 这个 stage 教什么

本阶段从 Stage04 软件基线进入 FFmpeg 硬件加速路径，按 `01~10` 拆解独立知识点：
1. `01_list_hwaccel_and_decoders.cpp`：列出 FFmpeg 构建可见硬件后端与 decoder 硬件配置。
2. `02_create_hwdevice_context.cpp`：最小 `av_hwdevice_ctx_create` 创建与失败定位，板端默认用 `drm`。
3. `03_get_hw_pixel_format.cpp`：默认检查 `h264_rkmpp` wrapper；显式 `HW_TYPE=...` 时才检查 `decoder + hw_type` 硬件像素格式。
4. `04_hwframe_vs_swframe.cpp`：默认走 `h264_rkmpp` decoder wrapper；显式 `HW_TYPE=...` 时才进入 hwdevice/hwframe 实验模式。
5. `05_hwdownload_transfer.cpp`：解释 `av_hwframe_transfer_data` 为什么会引入回拷。
6. `06_drm_prime_frame_note.cpp`：DRM PRIME / DMA-BUF 路径边界说明。
7. `07_hwaccel_benchmark_commands.cpp`：软硬路径 benchmark 命令模板。
8. `08_fallback_detection_checklist.cpp`：硬件回退识别清单。
9. `09_enterprise_ffmpeg_hwaccel_service`：企业级服务化验证（位于 `enterprise_project/`）。
10. `10_performance_diagnosis_playbook.cpp`：30分钟性能定位实操入口（先测量再优化）。

并追加企业级补充项目（`09`）在 `enterprise_project/`：
- 多模块服务化入口（CLI / 状态机 / 日志 / 指标 / gate / 故障注入矩阵）。
- 明确区分“硬件路径真实命中”与“软件 fallback 假象”。

## 对应岗位场景

1. SoC 多媒体适配时，定位“命令看起来像硬解，但 CPU 仍高”的场景。
2. 需要向驱动同学给出可复现证据：`hw frame 数量`、`transfer 行为`、`fallback 触发点`。
3. 输出可门禁的结构化结果（日志 + JSON），用于回归验证。

## 本 stage 不教什么

1. 不实现 V4L2 M2M ioctl 全状态机（Stage06）。
2. 不覆盖完整 DRM/KMS 显示流水线（Stage08/09）。
3. 不给出厂商私有寄存器/SDK 细节（需芯片资料）。

## 文件结构

```text
stage05_ffmpeg_hwaccel_path/
├── README.md
├── Makefile
├── build.sh
├── include/
│   └── 00_ffmpeg_hwaccel_common.hpp
├── src/
│   ├── 01_list_hwaccel_and_decoders.cpp
│   ├── 02_create_hwdevice_context.cpp
│   ├── 03_get_hw_pixel_format.cpp
│   ├── 04_hwframe_vs_swframe.cpp
│   ├── 05_hwdownload_transfer.cpp
│   ├── 06_drm_prime_frame_note.cpp
│   ├── 07_hwaccel_benchmark_commands.cpp
│   ├── 08_fallback_detection_checklist.cpp
│   └── 10_performance_diagnosis_playbook.cpp
├── scripts/
│   ├── run_01_*.sh ... run_08_*.sh
│   ├── run_10_performance_diagnosis_playbook.sh
│   ├── run_09_enterprise_ffmpeg_hwaccel_service.sh
│   ├── run_all_stage05.sh
│   └── collect_env.sh
├── docs/
│   ├── 00_start_here.md
│   ├── 01_master_study_manual.md
│   ├── 02_main_execution_guide.md
│   ├── 02_final_checklist.md
│   ├── 03_code_walkthrough.md
│   ├── 04_experiment_matrix.md
│   └── 05_metrics_interpretation.md
├── expected_output/
│   ├── 01_list_hwaccel_and_decoders.txt
│   ├── ...
│   ├── 10_performance_diagnosis_playbook.txt
│   └── 09_enterprise_ffmpeg_hwaccel_service.txt
├── samples/
├── logs/
└── enterprise_project/
```

## 数据流图

```text
input.mp4
  -> avformat_open_input
  -> avformat_find_stream_info
  -> avcodec_send_packet
  -> avcodec_receive_frame
      -> if frame.format == hw_pix_fmt: hw frame
           -> av_hwframe_transfer_data (hwdownload copy-back)
      -> else if wrapper mode + rkmpp decoder: wrapper cpu-visible frame
      -> else: software fallback frame
```

## 依赖环境

1. `g++`
2. `ffmpeg` / `ffprobe`
3. FFmpeg dev libs：`libavformat-dev libavcodec-dev libavutil-dev`
4. `pkg-config`

## 编译命令

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage05_ffmpeg_hwaccel_path
./build.sh all
```

如果要显式使用 Rockchip 自编译 FFmpeg（例如 `/opt/rockchip/ffmpeg-rockchip`），使用：

```bash
FFMPEG_PREFIX=/opt/rockchip/ffmpeg-rockchip ./build.sh all
```

## 运行命令

```bash
./scripts/run_all_stage05.sh
```

或按 demo 单独跑：

```bash
./scripts/run_02_create_hwdevice_context.sh
INPUT=./samples/sample.mp4 ./scripts/run_04_hwframe_vs_swframe.sh
INPUT=./samples/sample.mp4 MAX_FRAMES=120 LOOPS=5 ./scripts/run_10_performance_diagnosis_playbook.sh
```

## 参数说明（示例）

| 参数 | 作用 | 为什么需要 | 可改成什么 | 改动后观察什么 |
| --- | --- | --- | --- | --- |
| `INPUT` | 输入文件 | 触发真实 packet/frame 流 | 任意 H.264/H.265 文件 | 是否稳定 decode |
| `DECODER` | 解码器 | 默认走 RKMPP wrapper 主线 | `h264_rkmpp/hevc_rkmpp/h264` | 是否仍能解码、是否可能 fallback |
| `HW_TYPE` | 可选目标后端 | 仅在显式实验 hwdevice/hwfmt 时使用 | `drm/vaapi/rkmpp/v4l2m2m` | 设备创建与格式协商差异 |
| `DEVICE` | 可选后端设备节点 | 只在后端需要设备节点时指定 | `/dev/dri/renderD128` 等 | `av_hwdevice_ctx_create` 成败 |
| `MAX_FRAMES` | 最大解码帧数 | 限制日志体量，便于对比 | `16/120/1000` | 指标稳定性 |
| `LOOPS` | 重复轮次（demo10） | 做稳定性对比，避免偶发抖动 | `3/5/10` | 均值和方差是否稳定 |

## RK3568 当前建议

1. `h264_rkmpp` 能成功解码，说明 RKMPP 硬解路径已经可用。
2. Stage05 默认走 `h264_rkmpp` decoder wrapper；`HW_TYPE` 只作为显式 hwdevice 实验开关。
3. `02_create_hwdevice_context` 只验证 hwdevice 创建；它默认走 `drm`，不等于验证 `h264_rkmpp` 解码。
4. `vaapi` 出现在 `ffmpeg -hwaccels` 只说明 FFmpeg 编译了该后端；真实可用还要看 `libva/libva-drm`、平台 VA driver 和 `vainfo`。
5. 若 demo 二进制和命令行 `ffmpeg` 输出不一致，优先检查：

```bash
ldd ./bin/02_create_hwdevice_context | grep -E "libav|libdrm|libva"
ffmpeg -hide_banner -hwaccels
```

## 与 VM V4L2 M2M 学习的关系

1. VM 上的 V4L2 M2M 虚拟节点用于训练通用 Linux codec queue/state-machine。
2. RK 板上的真实硬件能力以 RKMPP 路径为准。
3. 报告里不要把 `h264_v4l2m2m`、VM 虚拟 M2M 或模拟结果写成 RKMPP 硬件证据。
4. 如果 RKMPP 输出 CPU 可见帧，不能只凭 `frame.format=yuv420p` 判定软件 fallback；要看 decoder 名、frame count、性能和日志。

## 当前环境限制（已实测）

当前机器若缺少 `ffmpeg/ffprobe` 或 `libav*` 开发包，`./build.sh` 会直接报错退出（前置依赖不满足，不是代码逻辑错误）。

安装示例（Ubuntu/Debian）：

```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev pkg-config
```

## 驱动影子线：这一阶段对应的驱动侧知识

1. FFmpeg `hwaccel` 最终可能通过 VAAPI/DRM（`/dev/dri/renderD*`）或 V4L2 M2M（`/dev/videoX`）触达内核。
2. 如果 `av_hwdevice_ctx_create` 失败，常见原因是设备节点、权限、驱动探测或后端不匹配。
3. 仅在显式 `HW_TYPE=...` hwdevice 实验模式中，`frame.format` 持续非硬件像素格式才可作为 fallback 证据。
4. 默认 `h264_rkmpp` wrapper 模式下，出现 CPU 可见帧（如 `yuv420p`）不等于软件解码器在工作。
5. `av_hwframe_transfer_data` 成功说明“可回拷”，但不代表“全链路零拷贝”。
6. `run_04` 以 `verdict` 作为最终判定：
   - `HARDWARE_FRAME_CONFIRMED`：硬件帧确认；
   - `HARDWARE_DECODE_WRAPPER_OUTPUT`：rkmpp 硬解 + CPU可见帧；
   - `SOFTWARE_FALLBACK`：软解回退；
   - `UNKNOWN_NEED_MORE_EVIDENCE`：证据不足。
7. `run_10` 生成 `summary.csv`，把 `decoder/verdict/帧计数` 与 `real/user/sys/cpu/maxrss` 放在同一行，直接做瓶颈归因。

## 如何扩展到下一阶段

1. Stage06：对接 V4L2 M2M ioctl 双队列模型，补齐 `QBUF/DQBUF` 驱动侧所有权链路。
2. Stage08/09：把 hw frame 与 DRM PRIME/DMA-BUF 共享路径打通，减少 `hwdownload` 回拷。


## 深度解析补充

- `docs/06_demo_deep_dive.md`：按 demo 的主流程/结构体/API/错误路径/驱动影子线深度解析。
