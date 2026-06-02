# Stage06 Code Walkthrough

## 主流程解析

基础 demo 的主线是：先看完整 decoder ioctl sequence，再拆成格式协商、buffer 生命周期、QBUF/DQBUF loop、SOURCE_CHANGE/EOS/drain、debug report。企业项目再把这些动作收敛成一个可 gate 的诊断服务。

## 关键结构体解析

| 结构体 | 字段重点 | 生命周期 | 工作中在哪里见到 |
| --- | --- | --- | --- |
| `v4l2_capability` | `driver/card/capabilities/device_caps` | `QUERYCAP` 成功后有效 | 设备节点发现、bring-up report |
| `v4l2_format` | `type/pixelformat/width/height/sizeimage/bytesperline` | `TRY_FMT/S_FMT` 前后对比 | 格式协商、resolution change |
| `QueueCounters` | qbuf/dqbuf/poll/timeout/source_change/eos | demo 单次运行 | 快速判断 queue loop 是否真的动了 |
| `PipelineMetrics` | counter + gate + failure_layer | 企业项目单次运行 | 自动化回归和 debug report 附件 |
| `StateMachine` | `PipelineState` history | 企业项目全流程 | 复盘状态机是否漏了 STREAMOFF/RECOVERY |

## 关键函数解析

| 函数 | 输入输出 | 所有权/状态变化 | 驱动影子线 |
| --- | --- | --- | --- |
| `xioctl(fd, request, arg)` | V4L2 fd + ioctl 参数 | 用户态进入驱动 ioctl 回调 | V4L2 ioctl ops |
| `open_video_node()` | `/dev/videoX` -> fd | 创建用户态 fd | driver open/session |
| `negotiate_one()` | queue type + fourcc + size | TRY/S_FMT 后驱动可回填格式 | driver format negotiation |
| `run_queue_loop()` | CLI config -> metrics | QBUF/DQBUF counter 变化 | vb2 buffer state + completion |
| `GateEvaluator::evaluate()` | config + metrics | 写入 pass/fail verdict | 把症状映射到诊断层级 |

## 数据流和所有权解析

```text
USER owns OUTPUT buffer
  -> fill compressed payload, bytesused > 0
  -> VIDIOC_QBUF OUTPUT
DRIVER owns OUTPUT buffer
  -> firmware/driver consumes bitstream
  -> VIDIOC_DQBUF OUTPUT
USER owns OUTPUT buffer again

USER owns CAPTURE buffer
  -> VIDIOC_QBUF CAPTURE empty buffer
DRIVER owns CAPTURE buffer
  -> VPU writes decoded frame
  -> IRQ/worker marks buffer done
  -> poll wakes user
  -> VIDIOC_DQBUF CAPTURE
USER owns decoded frame buffer again
```

## 错误路径和资源释放解析

1. `open` 失败：检查节点存在和权限。
2. `QUERYCAP` 成功但不是 M2M：不能当 codec 节点继续真实解码。
3. `S_FMT` 失败：检查 queue type、fourcc、尺寸、单/多平面。
4. `QBUF` 失败：检查 buffer index、bytesused、memory type、plane size。
5. `DQBUF` timeout：先看 OUTPUT/CAPTURE 是否都已 QBUF，再看 dmesg/IRQ/PM。
6. `SOURCE_CHANGE`：必须重配 CAPTURE queue，不能沿用旧 buffer。
7. cleanup：真实路径要 `STREAMOFF -> munmap -> REQBUFS 0 -> close`。

## 工作中对应的真实场景

1. FFmpeg V4L2 M2M 硬解 wrapper 不出帧。
2. GStreamer v4l2 decoder pipeline link 成功但运行后 timeout。
3. RK/高通/MTK 平台 codec 节点与 camera 节点混淆。
4. 720p 正常，切换 1080p 或码流中途分辨率变化后卡住。
5. EOS 后最后几帧没有输出。

## 下一步可以怎么改

1. 给 `02_format_negotiation_probe` 增加 `ENUM_FMT` 列表输出。
2. 给企业项目接入真实 `REQBUFS/MMAP`，先只申请 buffer 不解码。
3. 接入 Annex B parser，把每个 NALU 作为 OUTPUT payload 规划。
4. 真实 DQBUF 后 dump 前几帧 CAPTURE metadata。
