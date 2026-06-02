# Day03 缓冲队列与 MMAP

## 1. 今日目标

1. 吃透 `REQBUFS -> QUERYBUF -> mmap -> QBUF -> DQBUF -> QBUF`。
2. 理解为什么 `DQBUF` 后一定要 `QBUF` 回队。
3. 能口述“缓冲池生命周期”。

## 2. 必看知识

1. `study.md` 第 4、5、6.4、6.5、9 节。
2. `v4l2_capture_one_frame.cpp` 中 REQBUFS/QBUF/DQBUF 代码段。

## 3. 动手命令

```bash
./build.sh v4l2_capture_one_frame
./bin/v4l2_capture_one_frame /dev/video10 640 480 ../artifacts/day03_raw.yuyv ../artifacts/day03.ppm
```

## 4. 代码任务

1. 在 `DQBUF` 后打印：
   - `buf.index`
   - `buf.bytesused`
   - `buf.sequence`（如可用）
2. 连续跑 5 次，观察索引是否轮转。

## 5. 验收标准

1. 你能解释“为什么开流前要先把空缓冲都 QBUF”。
2. 你能解释“如果不回队会发生什么”。

## 6. 今日交付

1. 一段 8~12 行的缓冲流转解释。
