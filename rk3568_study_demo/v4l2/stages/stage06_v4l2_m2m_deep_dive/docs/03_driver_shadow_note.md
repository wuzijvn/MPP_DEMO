# 驱动影子线：这一阶段对应的驱动侧知识

## 用户态行为映射到哪个内核子系统

1. `/dev/videoX`：V4L2 character device，由具体 media/platform/camera/codec 驱动注册。
2. `VIDIOC_QUERYCAP`：V4L2 ioctl，返回 driver/card/capability。
3. `VIDIOC_S_FMT`：格式协商，进入驱动 format ops。
4. `REQBUFS/QUERYBUF/MMAP`：videobuf2 buffer 分配、查询、映射。
5. `QBUF/DQBUF`：vb2 buffer 所有权转移和完成回收。
6. `poll`：等待驱动 waitqueue，被 IRQ 或 worker completion 唤醒。
7. `SOURCE_CHANGE`：stateful decoder 在分辨率/格式变化时向用户态发事件。
8. `STREAMOFF`：停止队列和硬件 job，必须处理 active buffer cleanup。

## 哪些设备节点或 ioctl 涉及

| 目标 | 节点 | ioctl/API | 说明 |
| --- | --- | --- | --- |
| codec M2M | `/dev/videoX` | V4L2 M2M ioctls | 必须确认 M2M capability |
| camera/ISP | `/dev/videoX` | capture ioctls | 不是 codec decoder |
| media graph | `/dev/mediaX` | media controller | 帮助理解拓扑 |
| display/GPU | `/dev/dri/*` | DRM/KMS/PRIME | 后续 DMA-BUF/zero-copy 阶段 |

## 当前需要掌握

1. 不要把“能 open `/dev/video0`”等同于“找到 VPU codec”。
2. decoder OUTPUT 是 compressed bitstream，CAPTURE 是 decoded raw frame。
3. `bytesused` 是 OUTPUT payload 的生命线，错误会造成假 timeout。
4. `SOURCE_CHANGE` 后 CAPTURE queue 生命周期必须重新走。
5. timeout 需要分层，不要第一句话就定性 driver bug。

## 可以暂时推迟

1. 具体 VPU register 编程。
2. 厂商 firmware command 格式。
3. 完整 production VPU reset recovery。
4. DMA-BUF exporter/importer 内核实现细节。

## 驱动侧故障假设模板

| 用户态现象 | 可能驱动侧原因 | 需要证据 |
| --- | --- | --- |
| `DQBUF` timeout | IRQ 未到、job 未完成、firmware hang | dmesg、trace、queue counter |
| 第二次运行失败 | runtime PM/clock/reset 清理不完整 | PM 日志、重复 open/close 测试 |
| source change 后卡死 | CAPTURE queue 重配路径或事件处理问题 | DQEVENT、STREAMOFF/REQBUFS 日志 |
| 显示失败 | CAPTURE format/modifier 不被 DRM plane 支持 | DRM plane format、DMA-BUF 导入日志 |
| CPU 高 | hidden copy、hwdownload、格式转换 | copy-count、perf、FFmpeg/GStreamer log |
