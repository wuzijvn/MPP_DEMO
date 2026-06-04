# Stage07 Driver Shadow Note

## 1. element 到后端

GStreamer element 是用户态入口。常见映射：

| element 类型 | 可能后端 | 驱动/系统影子 |
| --- | --- | --- |
| `avdec_h264` | FFmpeg 软件解码 | CPU、libavcodec |
| `avdec_h264_rkmpp` | libav + RKMPP | Rockchip MPP/VPU、vendor library、dmesg |
| `v4l2*dec` | V4L2 codec | `/dev/videoX`、ioctl、vb2、V4L2 M2M |
| `vaapi*dec` | VAAPI | libva、DRM render node、Mesa/driver |
| `omx*dec` | OpenMAX | vendor OMX component |

## 2. caps 到驱动格式

GStreamer caps:

```text
video/x-raw,format=NV12,width=1920,height=1080,framerate=30/1
```

驱动侧常见对应：

```text
VIDIOC_ENUM_FMT
VIDIOC_TRY_FMT
VIDIOC_S_FMT
bytesperline
sizeimage
plane count
alignment/stride
```

codec caps:

```text
video/x-h264,stream-format=byte-stream,alignment=au,profile=high
```

硬件侧可能对应：

- parser 是否能提供 SPS/PPS；
- profile/level 是否支持；
- slice/tile/reference frame 约束；
- resolution change；
- DPB/firmware buffer 需求。

## 3. queue 到 buffer lifecycle

GStreamer `queue` 的用户态语义：

- 独立线程边界；
- 有限缓存；
- 满了就背压上游；
- 空了就等待下游/上游。

驱动影子：

- V4L2 OUTPUT/CAPTURE queue；
- vb2 buffer ownership；
- `QBUF -> driver owned`；
- `DQBUF -> user owned`；
- IRQ/worker 完成后唤醒 poll；
- queue depth 影响吞吐和 latency。

## 4. failure layer 快速判断

| 现象 | 通常层级 | 是否已到驱动 |
| --- | --- | --- |
| `no element` | rootfs/plugin | 否 |
| `could not link` | element pad/caps | 通常否 |
| `not-negotiated` | runtime caps/backend | 可能 |
| decoder open fail | backend/device | 可能 |
| timeout/hang | backend/driver/hardware/power | 可能 |
| EOS/drain 异常 | parser/backend/state machine | 可能 |

## 5. 硬解证据模板

```text
environment=RK_BOARD
framework=GStreamer
pipeline=...
backend_element=avdec_h264_rkmpp
codec=h264
input_stream=...
gst_result=EOS|ERROR|TIMEOUT
device_nodes=...
dmesg_hints=...
cpu_fps_result=...
fallback_check=...
what_this_proves=...
what_this_does_not_prove=...
next_step=...
```
