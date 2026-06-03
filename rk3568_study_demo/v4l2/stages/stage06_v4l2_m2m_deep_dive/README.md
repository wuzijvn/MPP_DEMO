# Stage06 - V4L2 M2M Deep Dive After FFmpeg Hwaccel

## 当前环境定位

Stage06 是“双环境桥接阶段”：

1. VM：深化 V4L2 M2M stateful decoder 状态机，训练 ioctl sequence、queue ownership、poll timeout、SOURCE_CHANGE、EOS/drain、fault matrix。
2. RK 板：先做 reality check。如果没有 V4L2 M2M codec 字符设备，Stage06 不要求真实 M2M decode；真实硬解验证回到 Stage05 RKMPP。
3. 本阶段的核心产出是“你能分清环境和 backend，并写出证据边界清楚的 debug report”。

完整双环境路线见：

```bash
less /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_dual_environment_codec_route.md
```

## RK 板端现实适配

当前 RK 板子可能没有可用的 V4L2 M2M codec 字符设备。请先运行：

```bash
./scripts/run_00_rk_board_reality_check.sh
```

如果输出 `v4l2_m2m_status=NOT_FOUND` 且 `rkmpp_status=AVAILABLE`，说明真实硬解主线应使用 RKMPP（例如 FFmpeg `h264_rkmpp/hevc_rkmpp`），Stage06 用来学习 V4L2 M2M 通用状态机和 debug 方法，不要强行对 rkisp/camera 节点跑 codec ioctl。详细说明见 [docs/05_rk_board_no_m2m_adaptation.md](docs/05_rk_board_no_m2m_adaptation.md)。

RK 板报告必须明确：

```text
environment=RK_BOARD
backend=rkmpp 或 no_v4l2_m2m_codec_node
hardware_proof=yes|no
what_this_proves=...
what_this_does_not_prove=...
```

## 这个 demo 教什么

Stage06 接在 Stage05 FFmpeg 硬解之后，目标是把“FFmpeg 可能走到硬件后端”继续落到 Linux V4L2 M2M codec 的真实工作语言：`OUTPUT/CAPTURE` 双队列、`S_FMT/REQBUFS/QBUF/DQBUF/STREAMON/STREAMOFF`、`poll timeout`、`SOURCE_CHANGE`、`EOS/drain`、以及如何写 driver-facing debug report。

基础 demo 负责把每个知识点拆开讲清楚；`enterprise_project/` 负责把同一条链路扩展成带状态机、指标 JSON、故障注入和 gate 的工作化诊断工具。

学的是同一条链路，企业级项目是在复杂度、可观测性、恢复策略上的扩展。

## 对应岗位场景

1. FFmpeg/GStreamer 硬解失败后，需要继续判断底层 `/dev/videoX` 是否是 codec M2M 节点。
2. V4L2 M2M decoder 可以 open/querycap，但 `STREAMON`、`poll` 或 `DQBUF` 超时。
3. 解码过程中发生分辨率变化，CAPTURE queue 没有正确重配。
4. EOS/drain 处理不完整，最后几帧丢失或 pipeline 卡住。
5. 需要向驱动工程师提交带命令、日志、counter、dmesg 检查项的 debug report。

## 本 demo 不教什么

1. 不实现完整真实硬解输出文件，因为这需要目标 SoC 的 codec M2M 节点、支持格式、码流样本和平台细节。
2. 不假设 `/dev/video0` 就是 codec 节点。当前实测 `/dev/video0` 是 `rkisp_v5/rkisp_mainpath`，不是 M2M codec。
3. 不发明 RK3568 私有寄存器、MPP 内部 ABI 或厂商 SDK 函数。
4. 不替代 Stage03 入门内容，而是在 Stage05 之后做 V4L2 M2M 深化。

## 文件结构

```text
stage06_v4l2_m2m_deep_dive/
├── README.md
├── Makefile
├── build.sh
├── include/00_stage06_m2m_common.hpp
├── src/
│   ├── 01_decoder_ioctl_sequence_map.cpp
│   ├── 02_format_negotiation_probe.cpp
│   ├── 03_mmap_buffer_lifecycle_sim.cpp
│   ├── 04_qbuf_dqbuf_poll_timeout_sim.cpp
│   ├── 05_source_change_eos_drain_sim.cpp
│   └── 06_timeout_debug_report_template.cpp
├── scripts/
│   ├── run_00_rk_board_reality_check.sh
│   ├── run_01_decoder_ioctl_sequence_map.sh
│   ├── run_02_format_negotiation_probe.sh
│   ├── run_03_mmap_buffer_lifecycle_sim.sh
│   ├── run_04_qbuf_dqbuf_poll_timeout_sim.sh
│   ├── run_05_source_change_eos_drain_sim.sh
│   ├── run_06_timeout_debug_report_template.sh
│   ├── run_07_enterprise_m2m_diagnostic_service.sh
│   ├── run_all_stage06.sh
│   └── collect_env.sh
├── docs/
│   ├── 00_start_here.md
│   ├── 01_code_walkthrough.md
│   ├── 02_experiment_matrix.md
│   ├── 03_driver_shadow_note.md
│   ├── 04_acceptance_checklist.md
│   └── 05_rk_board_no_m2m_adaptation.md
├── expected_output/
├── samples/
├── logs/
└── enterprise_project/
```

## 数据流图

```text
H.264/H.265 elementary stream packet
  -> OUTPUT QBUF, bytesused > 0, USER -> DRIVER
  -> V4L2 M2M driver / firmware parses bitstream
  -> CAPTURE empty frame buffer QBUF, USER -> DRIVER
  -> VPU job scheduled by v4l2-mem2mem/videobuf2
  -> IRQ or worker completion wakes poll/DQBUF
  -> CAPTURE DQBUF returns decoded NV12/YUV frame, DRIVER -> USER
  -> SOURCE_CHANGE may force CAPTURE queue reconfiguration
  -> EOS/drain flushes delayed reference frames from DPB
```

## 依赖环境

1. Linux userspace。
2. `g++` with C++11。
3. Linux V4L2 headers: `/usr/include/linux/videodev2.h`。
4. 可选：`v4l2-ctl`、`dmesg`，用于真实设备证据收集。

## 编译命令

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive
./build.sh all-with-enterprise
```

| 参数 | 作用 | 为什么需要 | 可改成什么 | 改动后观察什么 |
| --- | --- | --- | --- | --- |
| `all-with-enterprise` | 编译基础 demo 和企业项目 | 确保本阶段双轨都可运行 | `all` / `enterprise` / 单个源文件名 | 编译范围变化 |
| `CXX` | 指定 C++ 编译器 | 交叉编译或板端编译时常用 | `g++` / 交叉编译器 | 二进制架构和告警 |
| `CXXFLAGS` | 指定编译选项 | 调试时可加 `-g -O0` | `-std=c++11 -g -O0 -Wall` | 调试信息和优化行为 |

## 运行命令

基础 demo 全跑：

```bash
./scripts/run_all_stage06.sh
```

企业项目单跑：

```bash
./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

企业故障矩阵：

```bash
./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh
```

要求真实 M2M codec 节点 gate：

```bash
./enterprise_project/bin/07_enterprise_m2m_diagnostic_service \
  --device=/dev/video0 \
  --require-device \
  --output-dir=enterprise_project/logs/run_require_m2m_gate
```

## 参数说明

| 参数 | 作用 | 为什么需要 | 可改成什么 | 改动后观察什么 |
| --- | --- | --- | --- | --- |
| `DEVICE` / `--device` | 指定 V4L2 节点 | 验证是不是 codec M2M 节点 | `/dev/video0`、其他 video 节点 | `driver/card/m2m_capable` |
| `SIMULATE` / `--simulate` | 强制模拟格式协商 | 没有真实 codec 节点也能学习报告结构 | `0/1` | 是否访问真实节点 |
| `OUTPUT` / `--output-fourcc` | 压缩 OUTPUT 格式 | decoder OUTPUT queue 放 compressed bitstream | `H264/HEVC/VP80` 等 | 格式协商或报告字段 |
| `CAPTURE` / `--capture-fourcc` | raw CAPTURE 格式 | decoder CAPTURE queue 输出 decoded frame | `NV12/YU12` 等 | stride/sizeimage 风险 |
| `FRAMES` / `--frames` | 模拟帧数 | 控制 qbuf/dqbuf loop 长度 | `4/12/1000` | 计数器和稳定性 |
| `TIMEOUT_AT` / `--timeout-at` | 注入 poll timeout | 训练 DQBUF timeout 分层定位 | `-1/5` | timeout/recovery counter |
| `SOURCE_CHANGE_AT` / `--source-change-at` | 注入 source change | 训练 CAPTURE queue 重配 | `-1/4` | source_change/recovery counter |
| `--require-device` | 要求真实 M2M capability | 防止把 ISP capture 节点误判成 codec | 开/关 | `FAIL_M2M_CAPABILITY_REQUIRED` |

## 预期输出

基础全跑输出示例：

```text
[run] 01_decoder_ioctl_sequence_map
  PASS 01_decoder_ioctl_sequence_map
...
[run] 06_timeout_debug_report_template
  PASS 06_timeout_debug_report_template
[run_all] logs: .../logs/run_all_stage06_<timestamp>
```

企业项目输出示例：

```text
[INFO][querycap] driver=rkisp_v5, card=rkisp_mainpath, m2m_capable=no
[WARN][querycap] opened node is not a V4L2 M2M codec device; continue in simulated codec queue mode
[INFO][gate] decoded_frames=12, ... gate_pass=yes, verdict=PASS_NORMAL_PATH
enterprise_metrics=.../enterprise_metrics.json
```

## 每一行关键输出说明

| 输出 | 证明什么 | 如果异常看什么 |
| --- | --- | --- |
| `m2m_capable=no` | 当前节点不是 codec M2M | 换 `/dev/videoX`、查 `v4l2-ctl --list-devices` |
| `QBUF OUTPUT bytesused>0` | 压缩 payload 被交给驱动 | `bytesused=0`、码流头缺失 |
| `QBUF CAPTURE empty` | 空输出帧 buffer 交给驱动 | CAPTURE buffer 数量和 sizeimage |
| `poll timeout` | 驱动没有在期限内完成 buffer | bitstream、IRQ、firmware、runtime PM |
| `SOURCE_CHANGE` | 分辨率/stride 可能变化 | CAPTURE STREAMOFF/REQBUFS/S_FMT |
| `enterprise_verdict` | gate 结论 | JSON 中 `failure_layer` |

## 关键结构体

1. `v4l2_capability`：证明设备节点是什么驱动、什么 card、是否有 M2M capability。
2. `v4l2_format`：承载 OUTPUT/CAPTURE 格式协商，驱动可能回填 width/height/stride/sizeimage。
3. `v4l2_buffer`：真实 QBUF/DQBUF 时承载 index、bytesused、timestamp、sequence、flags。
4. `QueueCounters`：教学计数器，帮助你把队列行为变成可观察证据。
5. `PipelineMetrics`：企业项目 JSON 指标，直接用于 pass/fail gate。

## 关键函数

1. `xioctl()`：封装 ioctl 并处理 `EINTR` 重试。
2. `open_video_node()`：以 `O_NONBLOCK` 打开 V4L2 节点，方便 poll/DQBUF 状态机。
3. `negotiate_one()`：执行 `TRY_FMT` 或 `S_FMT`，观察驱动格式回填。
4. `run_queue_loop()`：企业项目中的核心 QBUF/poll/DQBUF 模拟 loop。
5. `GateEvaluator::evaluate()`：把 counter 转成客观 gate 结论。

## 资源生命周期

```text
open fd
  -> QUERYCAP
  -> TRY_FMT/S_FMT OUTPUT and CAPTURE
  -> REQBUFS/QUERYBUF/MMAP
  -> QBUF OUTPUT and CAPTURE
  -> STREAMON
  -> poll/DQBUF/QBUF loop
  -> SOURCE_CHANGE: STREAMOFF CAPTURE, REQBUFS 0, S_FMT, REQBUFS, QBUF, STREAMON
  -> EOS/drain
  -> STREAMOFF
  -> munmap
  -> REQBUFS count=0
  -> close fd
```

## 常见错误与解决方向

| 现象 | 最可能原因 | 属于哪一层 | 验证命令 | 解决方向 | 下一步看什么 |
| --- | --- | --- | --- | --- | --- |
| `/dev/video0` 能打开但硬解不通 | 节点是 ISP/camera，不是 codec M2M | 设备节点层 | `v4l2-ctl --list-devices` | 找 codec 节点 | `QUERYCAP` capability |
| `S_FMT` 返回 `EINVAL` | fourcc/尺寸/queue type 不支持 | 格式协商层 | `v4l2-ctl -d X --list-formats-ext` | 换格式或 mplane type | 驱动回填格式 |
| `DQBUF timeout` | payload、buffer、IRQ、firmware、PM 均可能 | 队列/驱动层 | 本阶段 demo06 报告模板 | 先证明 bytesused 和 QBUF 顺序 | dmesg/trace |
| source change 后卡住 | CAPTURE queue 没有重配 | 状态机层 | `--inject=source_change_no_reconfigure` | STREAMOFF CAPTURE 后重配 | sizeimage/stride |
| EOS 后丢尾帧 | 没有 drain DPB delayed frames | codec 流程层 | demo05 | EOS 后继续 DQBUF 到 LAST | buffer flags |
| CPU 仍高 | hidden copy/hwdownload 或没走硬件 | 框架/性能层 | 回看 Stage05 benchmark | 统计 copy count | DRM/DMA-BUF 后续阶段 |

## 调试 checklist

1. `ls -l /dev/video*`。
2. `v4l2-ctl --list-devices`。
3. `v4l2-ctl -d /dev/videoX --all`。
4. `v4l2-ctl -d /dev/videoX --list-formats-ext`。
5. 记录 OUTPUT/CAPTURE fourcc、buffer count、bytesused。
6. 记录每次 `QBUF/DQBUF` 的 index、sequence、timestamp、flags。
7. `dmesg | grep -Ei 'v4l2|m2m|vpu|rkvdec|codec|timeout|reset|iommu|dma|firmware'`。
8. 如果涉及性能，记录 qbuf/dqbuf counter、timeout_count、max queue depth。

## 优化方向

1. 把模拟 loop 扩展成真实 `REQBUFS/MMAP/QBUF/DQBUF`。
2. 接入 Annex B H.264/H.265 parser，真实填充 OUTPUT `bytesused`。
3. 记录 CAPTURE `sequence/timestamp/flags`，定位丢帧和乱序。
4. 增加 `trace-cmd` 或 ftrace 采集，关联用户态 timeout 与驱动 completion。
5. 后续接 DMA-BUF/DRM PRIME，减少 CAPTURE 到显示链路的 hidden copy。

## 驱动影子线

1. `open()` 对应驱动 `file_operations.open`，通常创建 per-session context。
2. `VIDIOC_QUERYCAP` 对应驱动 capability 报告，不能把任意 video 节点当 codec。
3. `VIDIOC_S_FMT` 对应格式协商，驱动可能调整尺寸、stride、sizeimage。
4. `REQBUFS/QUERYBUF/MMAP` 对应 videobuf2 buffer setup。
5. `QBUF` 把 buffer 所有权交给驱动；`DQBUF` 把完成 buffer 还给用户态。
6. `poll` 等待驱动 waitqueue wakeup，通常由 IRQ 或 worker completion 触发。
7. `SOURCE_CHANGE` 和 `EOS/drain` 是 stateful decoder 最容易出 bug 的状态机边界。

## 如何扩展到真实 SoC/VPU 场景

1. 先用 `v4l2-ctl --list-devices` 找到 codec M2M 节点，而不是 camera/ISP 节点。
2. 用 Stage03/Stage06 的格式探测确认 OUTPUT compressed format 和 CAPTURE raw format。
3. 准备 Annex B elementary stream，确保 SPS/PPS/VPS 存在。
4. 实现真实 OUTPUT payload QBUF 和 CAPTURE mmap buffer DQBUF。
5. 每次失败都输出 debug report，不直接写“driver bug”。

## 面试/入职表达模板

> V4L2 M2M decoder 有两个队列：OUTPUT 接收压缩码流，CAPTURE 返回解码后的 raw frame。用户态通过 QBUF 把 buffer 所有权交给驱动，通过 DQBUF 拿回完成 buffer。poll timeout 不一定是驱动 bug，我会先确认输入码流、bytesused、格式协商、CAPTURE buffer 数量和 SOURCE_CHANGE/EOS 处理，再结合 dmesg 判断 IRQ、firmware、runtime PM 或 reset recovery。
