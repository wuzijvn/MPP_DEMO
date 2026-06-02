# Stage04 总手册：FFmpeg Software Path

## 阶段目标

建立纯软件解码基线，掌握 `demux -> decode -> frame` 主链路及资源释放规范，为后续硬件路径对照与定位提供可重复证据。

## 你会做什么

1. 跑 `01~08` 拆分 demo。
2. 用 `07` 做故障注入，验证 cleanup 对称性。
3. 用 `08` 建立软件解码 fps 基线。
4. 跑 `09` 企业级项目，输出结构化日志和 metrics。

## 你会学到什么知识

1. `AVFormatContext/AVCodecContext/AVPacket/AVFrame` 的职责。
2. send/receive 解码模型。
3. packet/frame 所有权与释放时机。
4. `PTS/DTS/time_base` 的工程意义。
5. 为什么软件路径是硬件调试前置基线。

## 驱动影子线：这一阶段对应的驱动侧知识

1. 软件路径本身不走 `/dev/videoX`，但它提供硬件路径故障隔离对照。
2. 若软件路径稳定而硬件路径不稳定，优先看 hwaccel backend、驱动格式能力、设备节点和 dmesg。
3. 这阶段先不深入驱动实现，只建立分层定位方法。

## 对应岗位能力

1. 能独立编写 FFmpeg 纯软件 decode 样例。
2. 能解释 packet/frame 生命周期与泄漏风险。
3. 能产出可复盘日志与基线指标。

## 动手任务（必须可执行）

1. `./scripts/run_01_open_input_and_find_stream.sh`
2. `./scripts/run_03_decode_packet_to_frame.sh`
3. `./scripts/run_05_pts_dts_timebase.sh`
4. `./scripts/run_07_error_cleanup_pattern.sh INJECT_STEP=3`
5. `./scripts/run_08_cpu_decode_benchmark.sh MAX_FRAMES=240`
6. `./scripts/run_09_enterprise_ffmpeg_pipeline_service.sh`

## 验收标准（通过/不通过）

通过：
1. 能解释 `03` 中 send/receive 循环。
2. 能解释 `04/07` 的释放路径。
3. 能输出 `08` 基线 fps 并给出输入条件。
4. 能读懂企业级项目 metrics 字段。

不通过：
1. 只会跑命令，不知道 packet/frame 谁释放。
2. 不能说明 `EAGAIN/EOF` 在 receive 中含义。
3. 无法给出软件基线就直接谈硬件优化。

## 常见坑

1. 不调用 `av_packet_unref` 导致内存涨。
2. 忘记过滤 stream_index，导致把音频包送视频解码器。
3. 错把 `EAGAIN` 当错误退出。
4. 没有样本文件却误判为代码 bug。

## 面试表达模板

“我先用 FFmpeg 纯软件路径建立 decode baseline，把 `AVPacket/AVFrame` 生命周期、`PTS/DTS/time_base` 和错误清理跑通，再用同输入对比硬件路径，定位问题在应用层还是 hwaccel/驱动层。”

## 本阶段总结：通过这些例子你学到了什么

1. 核心知识：软件解码调用链和资源生命周期。
2. 驱动影子线：软件路径作为硬件路径定位对照。
3. 岗位映射：具备 baseline 构建与分层排障表达能力。
4. 可独立完成：纯软件 decode demo、基线测量、故障注入复盘。
5. 剩余缺口：硬件帧路径、设备节点、DRM/DMA-BUF 仍待 Stage05+ 深化。
