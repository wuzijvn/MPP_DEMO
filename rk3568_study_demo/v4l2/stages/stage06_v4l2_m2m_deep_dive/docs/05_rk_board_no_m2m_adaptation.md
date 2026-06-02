# RK Board Adaptation: No V4L2 M2M Codec Node

## 一句话结论

如果 RK 板子上没有真实 V4L2 M2M codec 字符设备，Stage06 不能按“真实 ioctl 硬解”来跑；应该把 Stage06 当作 V4L2 M2M 状态机/驱动影子线学习，把真实硬解验证切到 RKMPP 路径，例如 FFmpeg `h264_rkmpp` / `hevc_rkmpp`。

## 当前板端证据

本环境能看到：

```text
/dev/video0~9      rkisp / camera capture 相关节点
/dev/video10~11    USB camera 相关节点
/dev/video-dec0    普通文件，不是字符设备
/dev/video-enc0    普通文件，不是字符设备
ffmpeg decoders    h264_rkmpp/hevc_rkmpp/vp8_rkmpp/vp9_rkmpp/av1_rkmpp 可见
```

这意味着：

1. `/dev/video0` 不是 codec M2M decoder。
2. `/dev/video-dec0` 如果不是 `c` 开头的字符设备，就不能作为 V4L2 ioctl 节点使用。
3. FFmpeg 真实硬解更可能通过 RKMPP wrapper 进入 Rockchip MPP/BSP 栈。

## 为什么这不是学习失败

V4L2 M2M 是 Linux codec 栈的重要通用模型；但 RK BSP 常见真实路径是：

```text
ffmpeg -c:v h264_rkmpp
  -> FFmpeg rkmpp decoder wrapper
  -> Rockchip MPP userspace library / vendor media stack
  -> kernel rkvdec/rkvenc or vendor driver
  -> VPU hardware
```

而不是：

```text
ffmpeg -c:v h264_v4l2m2m
  -> /dev/videoX V4L2 M2M codec node
  -> VIDIOC_QBUF/DQBUF
```

所以 Stage06 的正确用法要分成两条：

| 目标 | 当前做法 |
| --- | --- |
| 学会 V4L2 M2M 模型 | 跑 Stage06 模拟 demo 和 enterprise fault matrix |
| 验证 RK 板真实硬解 | 回到 Stage05，用 `h264_rkmpp/hevc_rkmpp` 做证据化 benchmark |
| 写 driver-facing 报告 | 报告里说明 backend 是 RKMPP，不是 V4L2 M2M |

## 必跑命令

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive
./scripts/run_00_rk_board_reality_check.sh
```

输出重点：

```text
v4l2_m2m_status=NOT_FOUND
rkmpp_status=AVAILABLE
recommended_learning_path=RKMPP_REAL_PATH_PLUS_V4L2_M2M_CONCEPT
```

## 不要跑什么

不要对这些节点强行跑真实 codec M2M ioctl：

```text
/dev/video0~9   rkisp/camera capture
/dev/video10~11 USB camera
/dev/video-dec0 如果不是字符设备，也不能作为 V4L2 ioctl 节点
```

否则你看到的 `EINVAL`、`ENOTTY`、`not a device` 并不是 codec 硬件不支持，而是 backend/节点选错。

## 继续学习的正确姿势

1. Stage06 基础 demo：继续跑，用来掌握 `OUTPUT/CAPTURE/QBUF/DQBUF/SOURCE_CHANGE/EOS`。
2. Stage06 enterprise：继续跑 fault matrix，用来练习 debug report 和 gate。
3. Stage05 RKMPP：作为板端真实硬解验证主线。
4. 后续 Stage07 GStreamer：重点看是否有 `mppvideodec`、`mpph264dec` 或同类 RKMPP 插件，而不是只盯 `v4l2slh264dec`。
5. 后续 Stage09 DMA-BUF/DRM PRIME：看 RKMPP 输出是否能走 DRM PRIME/zero-copy，而不是 V4L2 DMABUF queue。

## 面试/入职表达模板

> 我在 RK 板上发现没有可用的 V4L2 M2M codec 字符设备，`/dev/video0` 是 rkisp/camera 节点，`/dev/video-dec0` 也不是可 ioctl 的字符设备。所以我不会强行用 `h264_v4l2m2m` 跑真实硬解，而会用 FFmpeg `h264_rkmpp/hevc_rkmpp` 验证真实硬件路径。V4L2 M2M 的 OUTPUT/CAPTURE、QBUF/DQBUF、SOURCE_CHANGE、EOS/drain 我仍然按通用 Linux codec 模型学习，用于理解其他 SoC 或 upstream driver，并辅助和驱动同学沟通 buffer/state-machine 问题。
