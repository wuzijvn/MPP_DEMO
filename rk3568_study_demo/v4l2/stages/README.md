# V4L2 Stages Index

按阶段组织学习与代码，避免文件散乱。

## 已有阶段

1. `stage01_v4l2_capture_foundation`
- 目标：V4L2 采集基础、统计、故障注入
- 入口：`stage01_v4l2_capture_main.cpp`
- 构建：
  ```bash
  cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation
  ./build.sh stage01_v4l2_capture_main
  ```

2. `stage02_v4l2_controls_stability`
- 目标：V4L2 控制项读写、线程解耦、稳定性跑测与恢复策略
- 入口：`stage02_v4l2_controls_stability_main.cpp`
- 构建：
  ```bash
  cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability
  ./build.sh stage02_v4l2_controls_stability_main
  ```

3. `stage03_v4l2_m2m_codec_foundation`
- 目标：V4L2 M2M 双队列模型入门（OUTPUT/CAPTURE）、最小编解码状态机骨架
- 入口：`src/01_open_video_device.cpp` ~ `src/10_source_change_eos_drain_note.cpp`（按编号渐进）
- 文档入口：`docs/00_start_here.md`（按编号顺序阅读）
- 构建：
  ```bash
  cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage03_v4l2_m2m_codec_foundation
  ./build.sh all
  ```

4. `stage04_ffmpeg_software_path`
- 目标：FFmpeg 纯软件路径学习与基线验证（demux/decode/ownership/pts-dts/cleanup/benchmark）
- 入口：`src/01_open_input_and_find_stream.cpp` ~ `src/08_cpu_decode_benchmark.cpp`
- 企业级补充：`enterprise_project/`（服务化入口 + metrics + fault injection）
- 构建：
  ```bash
  cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage04_ffmpeg_software_path
  ./build.sh all
  ```

5. `stage05_ffmpeg_hwaccel_path`
- 目标：FFmpeg 硬件加速路径学习与证据化验证（hwdevice/hwfmt/hwframe/fallback/hwdownload）
- 入口：`src/01_list_hwaccel_and_decoders.cpp` ~ `src/08_fallback_detection_checklist.cpp`
- 企业级补充：`enterprise_project/`（多模块服务 + gate + fault matrix）
- 构建：
  ```bash
  cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage05_ffmpeg_hwaccel_path
  ./build.sh all
  ```

6. `stage06_v4l2_m2m_deep_dive`
- 目标：Stage05 硬解之后的 V4L2 M2M 深化（ioctl 顺序、双队列所有权、poll timeout、SOURCE_CHANGE、EOS/drain、driver-facing report）
- 入口：`src/01_decoder_ioctl_sequence_map.cpp` ~ `src/06_timeout_debug_report_template.cpp`
- 企业级补充：`enterprise_project/`（CLI + 状态机 + 日志 + metrics JSON + gate + fault matrix）
- 当前边界：默认可模拟 codec queue loop；如果加 `--require-device`，会严格要求真实 M2M capability。
- 构建：
  ```bash
  cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive
  ./build.sh all-with-enterprise
  ```


## 命名规范

- 目录：`stageNN_<topic>`
- 代码组织：
  - 单入口型阶段可使用 `stageNN_<topic>_main.cpp`
  - 教学拆分型阶段（如 stage03）使用 `src/01_xxx.cpp ... src/NN_xxx.cpp`
- 公共头：`include/00_xxx_common.hpp`（或阶段前缀头）
- 文档：`docs/00_start_here.md` + 按编号文档
