# Day01 环境与节点识别

## 1. 今日目标

1. 确认你的 USB 摄像头对应哪个 `/dev/videoX`。
2. 能区分 `Video Capture`、`Metadata`、`Output` 节点。
3. 完成一次“命令行抓一帧”验证。

## 2. 必看知识

1. `study.md` 第 0、3、11 节。
2. 官方文档建议页：`common`、`querycap`（按你笔记中的阅读地图）。

## 3. 动手命令

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2
./v4l2_probe.sh
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video10 --all
v4l2-ctl -d /dev/video10 --list-formats-ext
```

命令行抓图验证：

```bash
v4l2-ctl -d /dev/video10 \
  --set-fmt-video=width=640,height=480,pixelformat=YUYV \
  --stream-mmap=4 --stream-count=1 --stream-to=../artifacts/day01.yuyv
```

## 4. 代码任务

1. 打开 `v4l2_capture_one_frame.cpp`，找出并标注以下步骤：
   - `QUERYCAP`
   - `S_FMT`
   - `REQBUFS`
   - `QBUF/DQBUF`
   - `STREAMON/OFF`

## 5. 验收标准

1. 你能说出“为什么用 `/dev/video10` 而不是 `/dev/video11`”。
2. 你能给出 `YUYV 640x480` 的理论帧大小：`614400 bytes`。
3. `../artifacts/day01.yuyv` 成功生成。

## 6. 今日交付

1. 一段你自己的节点判断结论（3~5 行）。
2. 一条抓图成功命令和输出截图/文本。
