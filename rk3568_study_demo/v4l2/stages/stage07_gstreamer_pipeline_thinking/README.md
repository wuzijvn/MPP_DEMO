# Stage07 - GStreamer Pipeline Thinking

Stage07 接在 Stage06 V4L2 M2M 深化之后，目标是把“底层 codec queue/state machine”上移到 GStreamer 这类 multimedia framework 的 pipeline 思维：element、pad、caps negotiation、queue、bus/error/EOS、GST_DEBUG、硬件后端候选与证据边界。

## 当前环境定位

本阶段默认可在当前 VM/RK Linux 上运行：

1. 基础 demo 使用 `gst-launch-1.0` / `gst-inspect-1.0`，不依赖 GStreamer C 开发头文件。
2. 默认输入使用 `videotestsrc` synthetic raw video，目的是学习 pipeline/caps/queue/debug，不声称跑了真实视频硬解。
3. RK 板如果安装了 `avdec_h264_rkmpp`、`avdec_hevc_rkmpp` 或 `mpph264enc`，本阶段会识别为 hardware/backend candidate；真正硬解证明仍需要成功解码真实码流、后端日志、dmesg/device node、CPU/fps/fallback 证据。

## 阶段目标

学会把 GStreamer pipeline 当作 SoC codec stack 的可观测图来分析：

- `source -> parser/demux -> decoder -> converter -> sink`
- `element pad` 如何通过 `caps` 连接
- `queue` 如何影响线程、背压、latency
- `GST_DEBUG` 如何采集可交接日志
- GStreamer failure 如何分层到 plugin/rootfs、caps、codec backend、driver/device
- 如何和 FFmpeg 路径互相对照

## 你会做什么

1. 探测 GStreamer 工具、基础 element、硬件后端候选。
2. 运行 raw caps negotiation pipeline。
3. 加入 queue 和慢节点，观察背压与耗时。
4. 采集 GST_DEBUG 日志。
5. 故意制造 link/caps failure 并分类。
6. 对照 FFmpeg 与 GStreamer 的 mental model。
7. 运行企业级 pipeline diagnostic service 和 fault matrix。

## 你会学到什么知识

- GStreamer element/pad/caps 的连接模型。
- capsfilter 与 converter 的职责差异。
- `queue` 的线程边界、缓存深度、latency tradeoff。
- `gst-inspect-1.0` 的 pad template 如何用于预判 link 是否可能。
- `GST_DEBUG` 日志如何作为 bring-up/debug report 证据。
- `decodebin` 自动选择和显式 decoder 选择的取舍。
- `avdec_h264_rkmpp` / `v4l2*` / `vaapi*` / `omx*` 这类 element 名称背后的后端含义。

## 驱动影子线：这一阶段对应的驱动侧知识

- caps 中的 `format/width/height/framerate` 对应驱动侧 `ENUM_FMT/TRY_FMT/S_FMT`、buffer layout、stride/alignment。
- hardware decoder element 可能进入 V4L2 M2M、RKMPP、VAAPI、OpenMAX 或厂商库；element 存在不是硬件证明。
- queue 深度对应真实 codec/display path 中的 OUTPUT/CAPTURE queue depth、vb2 buffer、IRQ completion、DQBUF 间隔。
- link failure 通常还没到 driver；runtime not-negotiated 或 backend error 才可能继续映射到 device node、ioctl、dmesg、firmware/power。
- GStreamer 与 FFmpeg 如果在同一码流同一后端都失败，优先怀疑 bitstream/profile/backend/driver support；如果只有 GStreamer 失败，优先比较 parser/caps/sink/memory type。

## 对应岗位能力

- 能写 GStreamer bring-up 命令和 failure report。
- 能判断 pipeline 失败属于命令、插件、caps、backend、driver 还是硬件证据不足。
- 能把 FFmpeg 成功/失败经验迁移到 GStreamer。
- 能向驱动工程师提供具体证据：pipeline、element、caps、日志、设备节点、dmesg、metrics。
- 能解释为什么 queue 影响吞吐和延迟，而不是简单等同于“性能优化”。

## 文件结构

```text
include/00_stage07_gst_common.hpp
src/01_gst_environment_probe.cpp
src/02_caps_negotiation_raw_video.cpp
src/03_queue_backpressure_latency.cpp
src/04_gst_debug_log_capture.cpp
src/05_link_failure_fault_injection.cpp
src/06_ffmpeg_gstreamer_compare.cpp
src/07_hardware_backend_probe.cpp
scripts/run_all_stage07.sh
docs/
expected_output/
enterprise_project/
```

## Demo Map

| Demo 文件 | 知识点 | 是否独立运行 | 运行命令 | 观察指标 | 常见错误 | 驱动影子线 |
| --- | --- | --- | --- | --- | --- | --- |
| `src/01_gst_environment_probe.cpp` | 工具和 element 探测 | 是 | `./scripts/run_01_gst_environment_probe.sh` | tool/element/backend candidate | 插件缺失、rootfs 不完整 | element 可见不等于硬件已跑 |
| `src/02_caps_negotiation_raw_video.cpp` | raw caps negotiation | 是 | `./scripts/run_02_caps_negotiation_raw_video.sh` | exit code、EOS、caps 参数 | capsfilter 被误认为转换器 | caps 映射 TRY_FMT/S_FMT |
| `src/03_queue_backpressure_latency.cpp` | queue/背压/latency | 是 | `./scripts/run_03_queue_backpressure_latency.sh` | elapsed_ms、queue depth | queue 过深导致延迟 | queue depth 映射 vb2/硬件队列 |
| `src/04_gst_debug_log_capture.cpp` | GST_DEBUG 日志 | 是 | `./scripts/run_04_gst_debug_log_capture.sh` | caps_mentions、log path | debug 太低看不到协商 | 日志需结合 dmesg |
| `src/05_link_failure_fault_injection.cpp` | link failure 分类 | 是 | `./scripts/run_05_link_failure_fault_injection.sh` | failure_layer | 把 link 错误甩给 driver | link 失败通常未到 ioctl |
| `src/06_ffmpeg_gstreamer_compare.cpp` | FFmpeg/GStreamer 对照 | 是 | `./scripts/run_06_ffmpeg_gstreamer_compare.sh` | 两套命令 exit code | 后端选择不一致 | 同后端对照定位层次 |
| `src/07_hardware_backend_probe.cpp` | 硬件后端候选 | 是 | `./scripts/run_07_hardware_backend_probe.sh` | found_hw_candidates | 插件存在当硬解证明 | 需要 dmesg/device/fallback 证据 |
| `enterprise_project/` | 企业级诊断服务 | 是 | `./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh` | JSON metrics、gate、fault matrix | 没有结构化证据 | 用户态 counters 映射后端/驱动排查 |

学的是同一条链路，企业级项目是在复杂度、可观测性、恢复策略上的扩展。

## 动手任务（必须可执行）

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage07_gstreamer_pipeline_thinking
./build.sh all-with-enterprise
./scripts/run_all_stage07.sh
./enterprise_project/scripts/run_07_enterprise_gst_pipeline_service.sh
./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh
```

## 验收标准（通过/不通过）

通过：

- `./build.sh all-with-enterprise` 成功。
- `run_all_stage07.sh` 生成 `logs/run_all_stage07_*`，基础 demo 输出 PASS verdict。
- 企业服务生成 `enterprise_metrics.json`，其中 `gate_pass=true`。
- fault matrix 至少包含 normal、caps failure、missing element、slow queue、hardware probe 五类。
- 你能解释 caps negotiation、queue、GST_DEBUG、failure layer、hardware proof boundary。

不通过：

- 只能跑命令但说不清 element/pad/caps。
- 把 `avdec_h264_rkmpp` installed 当作硬解已验证。
- 故障报告没有 pipeline、日志、metrics、设备/后端边界。
- queue latency 只看耗时，不解释背压和队列深度。

## 常见坑

- `capsfilter` 只是约束 caps，不负责转换。
- `decodebin` 自动选择 element，调试时最好先显式指定 decoder/parser。
- `GST_DEBUG` 输出多不代表定位完成；要筛 caps、pad、backend、error。
- raw test pipeline 跑通不能证明 H.264/H.265 硬解。
- hardware plugin 存在不能证明 VPU 工作；还要看真实码流、后端日志、dmesg/device node、CPU/fps/fallback。
- GStreamer sink 可能引入 hidden copy；后续 Stage08/Stage09 会继续进入 DRM/KMS、DMA-BUF、zero-copy。

## 面试表达模板

> 我会先用 `gst-inspect-1.0` 确认 element 和 pad template，再用最小 `gst-launch-1.0` pipeline 验证 caps 是否能 link。若 link 阶段失败，通常还没到驱动；若运行阶段出现 not-negotiated 或 backend error，我会打开 `GST_DEBUG`，同时收集 device node、dmesg 和后端日志。对于硬解路径，我不会只凭插件存在下结论，而会要求真实码流成功、后端 element 明确、CPU/fps/fallback 和驱动日志形成证据闭环。

## 本阶段总结：通过这些例子你学到了什么

1. 核心知识：GStreamer 是 element graph，pad 通过 caps 连接，queue 引入线程和缓存边界。
2. 驱动影子线：caps negotiation 失败可能映射到驱动 format/capability，但 link failure 多数仍在用户态。
3. 岗位映射：你能写出板端 pipeline smoke test、收集 GST_DEBUG、输出 failure layer。
4. 独立能力：你可以判断一个 GStreamer 问题属于 plugin/caps/backend/driver/hardware proof 哪一层。
5. 面试表达：你能把 FFmpeg 和 GStreamer 两套路径互相翻译，并说明硬解证据边界。
6. 剩余缺口：下一阶段进入 DRM/KMS、Mesa、VAAPI/VDPAU/OpenMAX，之后再深入 DMA-BUF/zero-copy。
