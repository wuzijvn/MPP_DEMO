# Driver Shadow Note

## VM/vim2m 映射

| 用户态动作 | 内核/驱动侧影子 |
| --- | --- |
| `open("/dev/video0")` | V4L2 character device open，创建 file/session context |
| `VIDIOC_QUERYCAP` | 驱动返回 `driver/card/device_caps` |
| `VIDIOC_ENUM_FMT` | 驱动暴露 OUTPUT/CAPTURE 支持格式 |
| `VIDIOC_S_FMT` | format ops，驱动可调整 fourcc、stride、sizeimage |
| `VIDIOC_REQBUFS` | videobuf2 queue 创建 buffer 槽位 |
| `VIDIOC_QUERYBUF` + `mmap` | 用户态映射 vb2 buffer |
| `VIDIOC_QBUF` | buffer 所有权 USER -> DRIVER |
| `VIDIOC_STREAMON` | queue 进入 streaming，M2M scheduler 可调度 job |
| `poll` | 等待 waitqueue，被 job completion/worker 唤醒 |
| `VIDIOC_DQBUF` | buffer 所有权 DRIVER -> USER |
| `VIDIOC_STREAMOFF` | 停止 queue，回收 active buffer |
| `munmap` + `REQBUFS 0` | 释放用户映射和 vb2 buffer |

## RK/RKMPP 映射

RK 板如果没有 V4L2 codec M2M 节点，硬解证据应来自：

```text
FFmpeg h264_rkmpp/hevc_rkmpp
  -> Rockchip MPP userspace/library
  -> kernel VPU/media driver
  -> VPU hardware
```

这条路径不等同于 VM `vim2m`，也不要求对 `/dev/video0` 跑 codec M2M ioctl。

## 关键判断

1. `m2m_capable=yes` 只说明这个节点是 M2M，不说明它是 H.264/H.265 decoder。
2. `vim2m` 完成 `DQBUF` 说明 M2M queue 逻辑正确，不说明硬解成功。
3. `S_FMT(H264)` 在 raw M2M 设备上可能失败，也可能回填成 RGBP；要看 ioctl 返回和回填值。
4. `bytesused=0` 如果被 driver 接受，也仍然是用户态 payload 风险，需要继续看 poll/DQBUF 进展。
5. source change 的训练重点是 CAPTURE queue 重配顺序：`STREAMOFF -> REQBUFS 0 -> S_FMT -> REQBUFS -> QBUF -> STREAMON`。

## Timeout 分层

| 现象 | 第一层排查 |
| --- | --- |
| poll timeout | OUTPUT/CAPTURE 是否都已 QBUF |
| OUTPUT DQBUF 不增长 | payload/bytesused/format 是否合理 |
| CAPTURE DQBUF 不增长 | CAPTURE buffer 数量、sizeimage、driver completion |
| 第二次运行失败 | STREAMOFF、munmap、REQBUFS 0、runtime PM cleanup |
| RKMPP 命令失败 | FFmpeg decoder 是否存在、MPP 库、dmesg VPU/firmware 日志 |
