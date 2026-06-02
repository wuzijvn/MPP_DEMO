# Day05 连续采集与 FPS

## 1. 今日目标

1. 把“单帧程序”扩展为“连续采集 N 帧”。
2. 输出整体耗时和 FPS。
3. 学会记录超时次数。

## 2. 必看知识

1. `study.md` 第 13 节。
2. 你代码中 `select + DQBUF + QBUF` 循环段。

## 3. 动手任务

1. 给程序新增参数：`frames`（默认 100）。
2. 循环抓取 `frames` 帧，不再只保存第 4 帧。
3. 每帧可只统计，不必每帧落盘。

## 4. 指标输出建议

1. `frames_total`
2. `elapsed_ms`
3. `fps = frames_total / elapsed_s`
4. `select_timeout_count`
5. `dqbuf_fail_count`

## 5. 验收标准

1. 跑 100 帧成功。
2. 输出 FPS 和异常计数。
3. 你能解释 fps 低时优先查哪三项（格式、分辨率、CPU/IO）。

## 6. 今日交付

1. 一段连续采集输出日志（关键行）。
