# Day06 参数控制与图像稳定

## 1. 今日目标

1. 认识 V4L2 控制项（曝光、亮度、对比度等）。
2. 学会命令行查询和设置控制项。
3. 观察控制项对图像结果的影响。

## 2. 必看知识

1. `study.md` 第 8、12 节。
2. 官方文档 `control` 相关章节（按你的阅读地图）。

## 3. 动手命令

```bash
v4l2-ctl -d /dev/video10 --list-ctrls
v4l2-ctl -d /dev/video10 --list-ctrls-menus
```

示例（按设备支持情况调整）：

```bash
v4l2-ctl -d /dev/video10 --set-ctrl=brightness=128
v4l2-ctl -d /dev/video10 --set-ctrl=contrast=64
```

采集对比图：

```bash
./bin/v4l2_capture_one_frame /dev/video10 640 480 ../artifacts/day06_a.yuyv ../artifacts/day06_a.ppm
# 调整控制项后再抓
./bin/v4l2_capture_one_frame /dev/video10 640 480 ../artifacts/day06_b.yuyv ../artifacts/day06_b.ppm
```

## 4. 代码任务（可选）

1. 在程序里加入 `VIDIOC_QUERYCTRL` / `VIDIOC_G_CTRL` 示例函数（不要求全实现）。

## 5. 验收标准

1. 至少成功修改 1 个控制项并观察到图像变化。
2. 能说出“自动曝光开启时，手动曝光控制可能无效”。

## 6. 今日交付

1. 两张对比图 + 参数记录表。
