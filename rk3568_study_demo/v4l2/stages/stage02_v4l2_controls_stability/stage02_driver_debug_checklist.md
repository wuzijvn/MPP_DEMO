# Stage02 驱动渗透 03：驱动侧排障 Checklist（基础版）

> 目标：当你看到用户态 summary 异常时，知道下一步该看什么，而不是盲猜。

## A. 用户态先确认（必须先做）

1. 节点是否选对：`v4l2-ctl --list-devices`
2. 能力是否匹配：`v4l2-ctl -d /dev/videoX --all`
3. 格式是否被回退：看程序里 `S_FMT/G_FMT` 输出
4. 是否有 timeout/dq_fail/qbuf_fail

---

## B. 内核日志与设备状态

1. `dmesg -T | tail -n 200`
2. 看是否有：timeout、reset、irq error、iova fault、dma mapping error
3. 记录异常发生时间点与用户态日志时间对齐

---

## C. 现象 -> 首查方向

1. `select timeout` 增长
- 首查：驱动是否还有完成中断
- 次查：用户态是否回队不足（QBUF节奏）

2. `dq_fail` 增长
- 首查：是否大量 EAGAIN（短暂无帧）
- 次查：是否非 EAGAIN 严重错误（设备状态）

3. `qbuf_fail` 增长
- 首查：buffer 状态机是否错乱
- 次查：格式/sizeimage/bytesused 边界

4. `error_flag_frames` 增长
- 首查：硬件处理异常、中断错误、数据源异常

---

## D. 与驱动回调的映射（记住这张）

1. `QUERYCAP` -> `vidioc_querycap`
2. `S_FMT/G_FMT` -> `vidioc_s/g_fmt_*`
3. `REQBUFS` -> `vb2 queue_setup`
4. `QBUF` -> `vb2 buf_prepare/buf_queue`
5. `STREAMON` -> `vb2 start_streaming`
6. `DQBUF` -> done queue / 完成中断路径
7. `STREAMOFF` -> `vb2 stop_streaming`

---

## E. 快速报告模板（你以后给同事/导师）

1. 现象：
2. 复现命令：
3. 设备信息（driver/card/caps）：
4. 关键 summary 指标：
5. dmesg 关键行：
6. 初步判断层级（用户态/驱动/硬件）：
7. 下一步验证动作：

---

## 本阶段总结：通过这些例子你学到了什么

1. 你知道了异常先看用户态证据，再看内核证据。
2. 你能把 summary 指标映射到潜在驱动问题点。
3. 你具备了基础的“可交付排障报告”结构。
