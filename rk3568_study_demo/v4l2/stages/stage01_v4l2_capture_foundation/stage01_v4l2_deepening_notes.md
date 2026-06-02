# Stage01 深化说明（岗位导向）

> 这份说明只回答一个问题：当前 demo 里，哪些点最值得你深挖，为什么它们和你岗位直接相关。

## 1. 你必须深挖的 5 个点（按岗位价值排序）

1. 格式协商闭环：`ENUM_FMT/ENUM_FRAMESIZES/ENUM_FRAMEINTERVALS + TRY_FMT + S_FMT + G_FMT`
- 你岗位后续做 VPU 适配时，90% 的链路问题都和“格式协商不一致”有关。
- 本阶段已加：`--dump-formats` + `VIDIOC_TRY_FMT`。

2. stride/bytesperline/sizeimage
- 驱动经常会做行对齐（padding），不能假设紧凑内存。
- 本阶段已加：`save_ppm_from_yuyv_with_stride`，按 `bytesperline` 转换。

3. 流式队列语义（QBUF/DQBUF）
- 这是后续 `vb2` 驱动回调的用户态镜像。
- 本阶段已有：`skip-requeue` 故障注入，直观看“队列耗尽”。

4. 可观测性指标体系
- `fps/timeout/dq_fail/requeue_fail/bytesused分布/sequence_gap`
- 这是你后续性能调优和异常恢复的最小证据链。

5. 错误分级与恢复思路
- 现在你已区分 `select timeout`、`DQ EAGAIN`、`S_PARM non-fatal`。
- 下一步要补：热插拔（ENODEV）与自动重建流程。

## 2. 本次在 stage01 上新增的“深化能力”

1. 可选像素格式参数：`--pixfmt=FOURCC`
- 例如 `YUYV/NV12/MJPG`。
- 作用：你可以主动试探设备支持，而不是固定写死 YUYV。

2. 格式能力枚举：`--dump-formats`
- 打印设备支持的 `format/size/fps` 组合。
- 作用：给后续 FFmpeg/GStreamer 选型提供依据。

3. `VIDIOC_TRY_FMT` 探测
- 在真正 `S_FMT` 前看驱动建议值。
- 作用：发现“请求值被就近调整”的真实行为。

4. stride 感知的 PPM 导出
- 优先用 `bytesperline` 路径转图。
- 作用：减少图像错位误判，提升排障准确性。

5. 逐帧 trace CSV + jitter/timestamp/flags 统计
- 新增参数：`--trace-csv`、`--log-every`。
- 作用：把“感觉卡顿/掉帧”变成可量化证据。

## 3. 你接下来 3 天就做这三组实验

1. 实验A：同分辨率不同 pixfmt
```bash
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 ../../../artifacts/a_yuyv.yuyv ../../../artifacts/a_yuyv.ppm 120 --pixfmt=YUYV --dump-formats
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 ../../../artifacts/a_nv12.yuyv ../../../artifacts/a_nv12.ppm 120 --pixfmt=NV12 --no-save
```

2. 实验B：同格式不同分辨率
```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 ../../../artifacts/b_640.yuyv ../../../artifacts/b_640.ppm 300 --pixfmt=YUYV
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 ../../../artifacts/b_720.yuyv ../../../artifacts/b_720.ppm 300 --pixfmt=YUYV
```

3. 实验C：漏回队故障
```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 ../../../artifacts/c_skip.yuyv ../../../artifacts/c_skip.ppm 300 --inject=skip-requeue --inject-frame=30
```

4. 实验D：trace/jitter 分析
```bash
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 ../../../artifacts/d_raw.yuyv ../../../artifacts/d_view.ppm 300 --pixfmt=YUYV --trace-csv=../../../artifacts/d_trace.csv --log-every=30
```

5. 实验E：一键矩阵基线
```bash
./stage01_experiment_matrix.sh --dev=/dev/video10 --build
```

## 4. 做完实验你要能回答的岗位问题

1. 为什么请求分辨率和实际分辨率可能不一致？
2. `bytesperline` 和 `width*bytes_per_pixel` 为什么会不同？
3. 为什么 DQ 后必须 Q？
4. `TRY_FMT` 和 `S_FMT` 分别解决什么问题？
5. 如何用日志证明是“协商问题”而不是“驱动崩溃”？
