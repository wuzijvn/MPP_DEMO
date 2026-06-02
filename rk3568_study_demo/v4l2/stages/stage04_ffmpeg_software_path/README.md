# Stage04 - FFmpeg Software Path（完整教学包）

## 这个 stage 教什么

本阶段聚焦 FFmpeg 纯软件路径（不走硬件加速），按 `01~08` 独立 demo 拆解：
1. `01` 打开输入并识别流。
2. `02` demux packet 循环观察。
3. `03` send/receive 解码主流程。
4. `04` `AVPacket/AVFrame` 所有权与生命周期。
5. `05` PTS/DTS/time_base 可见化。
6. `06` 保存首帧 YUV420P。
7. `07` goto 清理与故障注入。
8. `08` 软件解码基线 fps。

并追加企业级补充项目（`09`）在 `enterprise_project/`：
- 服务化入口、结构化日志、metrics、故障注入矩阵、门禁判定。

## 对应岗位场景

1. 入职初期做 SoC 多媒体栈时，先建立软件路径基线，避免硬件问题和代码问题混淆。
2. 排查“硬解异常/性能异常”前，需要明确软件解码行为、时戳和资源释放逻辑。
3. 输出可复盘日志与基准数据，为后续 Stage05 硬件路径对比做基线证据。

## 本 stage 不教什么

1. 不覆盖 `AVHWDeviceContext/AVHWFramesContext`（那是 Stage05）。
2. 不覆盖 V4L2 M2M ioctl 细节（Stage06）。
3. 不覆盖 DRM/DMA-BUF 显示零拷贝（Stage08/09）。

## 文件结构

```text
stage04_ffmpeg_software_path/
├── README.md
├── Makefile
├── build.sh
├── include/
│   └── 00_ffmpeg_demo_common.hpp
├── src/
│   ├── 01_open_input_and_find_stream.cpp
│   ├── 02_demux_packet_loop.cpp
│   ├── 03_decode_packet_to_frame.cpp
│   ├── 04_packet_frame_ownership.cpp
│   ├── 05_pts_dts_timebase.cpp
│   ├── 06_save_yuv_frame.cpp
│   ├── 07_error_cleanup_pattern.cpp
│   └── 08_cpu_decode_benchmark.cpp
├── scripts/
│   ├── run_01_*.sh ... run_08_*.sh
│   ├── run_09_enterprise_ffmpeg_pipeline_service.sh
│   ├── run_all_stage04.sh
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
│   ├── 01_open_input_and_find_stream.txt
│   ├── ...
│   └── 09_enterprise_ffmpeg_pipeline_service.txt
├── samples/
├── logs/
└── enterprise_project/
```

## 数据流图

```text
input.mp4
  -> avformat_open_input
  -> avformat_find_stream_info
  -> av_read_frame (AVPacket)
  -> avcodec_send_packet
  -> avcodec_receive_frame (AVFrame)
  -> (optional) save yuv / benchmark / metrics
```

## 依赖环境

1. `g++`
2. `ffmpeg` / `ffprobe`
3. FFmpeg dev libs：`libavformat-dev libavcodec-dev libavutil-dev libswscale-dev`
4. `pkg-config`

## 编译命令

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage04_ffmpeg_software_path
./build.sh all
```

## 运行命令

```bash
./scripts/run_all_stage04.sh
```

或按 demo 单独跑：

```bash
./scripts/run_03_decode_packet_to_frame.sh INPUT=./samples/sample.mp4
```

## 当前环境限制（已实测）

当前机器（2026-05-18）缺少 `ffmpeg/ffprobe` 和 `libav*` dev 包，`./build.sh` 会提示缺依赖并退出。这不是代码逻辑错误，而是环境前置条件不满足。

安装示例（Ubuntu/Debian）：

```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev pkg-config
```

## 驱动影子线

1. 本阶段虽然是纯软件 decode，但它是硬件路径调试的“对照组”。
2. 当 Stage05 硬件路径异常时，先看 Stage04 是否能稳定解同一输入：
   - Stage04 成功而 Stage05 失败，问题更可能在 hwaccel backend、设备节点或驱动。
   - Stage04 也失败，优先查输入文件、容器、时戳或应用层逻辑。

## 如何扩展到下一阶段

1. Stage05：复用 `03/04/05/08` 的观测口径，加硬件路径对照。
2. Stage05 企业级项目可直接继承 Stage04 enterprise 的日志/metrics schema。


## 深度解析补充

- `docs/06_demo_deep_dive.md`：按 demo 的主流程/结构体/API/错误路径/驱动影子线深度解析。
