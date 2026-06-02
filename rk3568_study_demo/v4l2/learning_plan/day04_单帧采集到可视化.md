# Day04 单帧采集到可视化

## 1. 今日目标

1. 稳定保存 raw 和 ppm。
2. 理解 YUYV 到 RGB 的转换路径。
3. 能检查输出文件是否“尺寸合理”。

## 2. 必看知识

1. `study.md` 第 10 节。
2. `v4l2_capture_one_frame.cpp` 中 `yuyv_to_rgb24` 和 `save_ppm_from_yuyv`。

## 3. 动手命令

```bash
./build.sh v4l2_capture_one_frame
./bin/v4l2_capture_one_frame /dev/video10 640 480 ../artifacts/day04.yuyv ../artifacts/day04.ppm
ls -lh ../artifacts/day04.yuyv ../artifacts/day04.ppm
```

可选：

```bash
ffmpeg -y -f rawvideo -pixel_format yuyv422 -video_size 640x480 \
  -i ../artifacts/day04.yuyv -frames:v 1 ../artifacts/day04.jpg
```

## 4. 代码任务

1. 添加 `expected_bytes = width*height*2` 校验日志。
2. 当 `bytesused < expected_bytes` 时打印警告。

## 5. 验收标准

1. yuyv 文件大小接近理论值。
2. ppm 能正常打开查看。
3. 你能解释 YUYV 一组 4 字节对应 2 像素。

## 6. 今日交付

1. 一张你抓到的图和 3 行结论。
