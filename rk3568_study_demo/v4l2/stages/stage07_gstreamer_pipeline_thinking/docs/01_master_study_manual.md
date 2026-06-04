# Stage07 Master Study Manual

## 1. GStreamer 的基本模型

GStreamer pipeline 是一张有方向的图：

```text
source -> demux/parser -> decoder -> converter -> sink
```

每个节点叫 element。element 通过 pad 连接。pad 能接什么数据由 caps 描述，例如：

```text
video/x-raw,format=NV12,width=320,height=240,framerate=30/1
video/x-h264,stream-format=byte-stream,alignment=au
```

工作中最重要的不是背命令，而是看到错误时能回答：

- 哪两个 element 没连上？
- 哪个 pad template 不接受当前 caps？
- 数据现在是 compressed bitstream 还是 raw frame？
- 后端 element 是软件 decoder，还是 RKMPP/V4L2/VAAPI/OpenMAX 候选？

## 2. caps negotiation 深挖

`capsfilter` 的职责是约束格式，不是转换格式：

```bash
videotestsrc ! video/x-raw,format=NV12 ! videoconvert ! video/x-raw,format=I420 ! fakesink
```

这里真正执行转换的是 `videoconvert`。如果去掉 `videoconvert`，上下游不一定能协商成功。

驱动影子线：

- GStreamer caps negotiation 对 V4L2 后端常映射到 `VIDIOC_TRY_FMT` / `VIDIOC_S_FMT`。
- raw format 不只是 fourcc，还包括 stride、alignment、plane layout。
- codec caps 还可能包含 H.264 profile/level、stream-format、alignment；硬件不支持时可能表现为 not-negotiated、backend error 或 fallback。

## 3. queue 的意义

`queue` 会创建线程边界和有限缓存。它常用于：

- 隔离上下游处理速度；
- 观察背压；
- 防止某个慢 sink 阻塞整个 pipeline；
- 在 demux 后给 audio/video 分支分配独立线程。

但 queue 不是“自动变快”。如果下游慢，整体吞吐仍受慢节点限制；queue 改变的是调度边界、buffering 和 latency。

驱动影子线：

- V4L2 M2M 也有 OUTPUT/CAPTURE queue depth。
- 硬件 decoder 的吞吐取决于输入队列、输出队列、IRQ/worker completion、内存带宽、cache sync、clock/DVFS。
- queue 过深会增加 latency 和内存占用；queue 太浅可能 starve。

## 4. GST_DEBUG 的使用策略

建议从低到高：

```bash
GST_DEBUG='GST_CAPS:3,GST_ELEMENT_PADS:3,pipeline:3' gst-launch-1.0 ...
GST_DEBUG='*:2,GST_CAPS:4,v4l2*:4,libav*:4' gst-launch-1.0 ...
```

日志要和以下信息放在一起看：

- 完整 pipeline；
- `gst-inspect-1.0 <element>` pad template；
- 后端候选 element；
- `/dev/video*`、`/dev/dri/*`；
- dmesg media hints；
- CPU/fps/fallback 证据。

## 5. GStreamer 与 FFmpeg 对照

| 任务 | FFmpeg 表达 | GStreamer 表达 |
| --- | --- | --- |
| 输入 | `-i input.mp4` | `filesrc ! demux` 或 `uridecodebin` |
| 解码器 | `-c:v h264_rkmpp` | `h264parse ! avdec_h264_rkmpp` |
| 软件解码 | `h264` decoder | `avdec_h264` |
| raw 转换 | `-vf format=...` | `videoconvert ! video/x-raw,...` |
| 空输出 | `-f null -` | `fakesink sync=false` |
| 硬件帧下载 | `hwdownload` | backend memory caps + converter/sink 行为 |

如果 FFmpeg 能硬解而 GStreamer 失败，优先查：

- GStreamer 是否用了同一后端；
- parser caps 是否一致；
- sink 是否要求不兼容的 memory type；
- 是否发生 hidden copy 或 fallback；
- debug 日志是否显示 not-negotiated。

## 6. 本阶段最重要的证据边界

`gst-inspect-1.0 avdec_h264_rkmpp` 成功只证明：

```text
rootfs 中有该 GStreamer/libav element
```

它不证明：

```text
真实 H.264 码流已进入 VPU
解码没有 fallback
输出 buffer 没有 hidden copy
显示链路是 zero-copy
```

硬解证明至少需要：

1. 真实压缩码流成功解码。
2. pipeline 显式或日志明确选中了硬件后端。
3. dmesg/backend log/device node 有对应活动。
4. CPU/fps/功耗等指标符合硬件路径预期。
5. 报告写清楚 what this proves / what this does not prove。
