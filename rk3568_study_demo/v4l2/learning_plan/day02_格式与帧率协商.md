# Day02 格式与帧率协商

## 1. 今日目标

1. 理解 `S_FMT` 后“驱动可能改你参数”。
2. 理解 `S_PARM` 不是所有设备都严格支持。
3. 学会对比“请求参数 vs 生效参数”。

## 2. 必看知识

1. `study.md` 第 6.2、6.3、8 节。
2. `v4l2_capture_one_frame.cpp` 中 `S_FMT` 和 `S_PARM` 段落。

## 3. 动手命令

```bash
v4l2-ctl -d /dev/video10 --list-formats-ext
v4l2-ctl -d /dev/video10 --set-fmt-video=width=640,height=480,pixelformat=YUYV
v4l2-ctl -d /dev/video10 --all
v4l2-ctl -d /dev/video10 --set-parm=30
v4l2-ctl -d /dev/video10 --all
```

## 4. 代码任务

1. 在程序里新增 `G_FMT` 打印（若你还没做）。
2. 运行并记录：
   - 请求分辨率
   - 实际分辨率
   - 请求 fps
   - 实际 fps（若驱动可返回）

## 5. 验收标准

1. 你能解释“为什么必须打印 active fmt”。
2. 你能举出一个“驱动改参”的例子（即使没改，也要说明如何判断）。

## 6. 今日交付

1. 参数对比表（请求值/生效值）。
