# Stage02 驱动渗透 01：用户态调用到驱动回调映射（入门版）

> 目标：你现在虽然先在用户态写 demo，但要马上建立“这行 ioctl 在驱动里是谁处理”的意识。

## 1. 为什么要学这个映射

你现在看到的都是：

1. `VIDIOC_QUERYCAP`
2. `VIDIOC_S_FMT`
3. `VIDIOC_REQBUFS`
4. `VIDIOC_QBUF`
5. `VIDIOC_DQBUF`
6. `VIDIOC_STREAMON/OFF`

如果只记这些名字，你在工作里排障会卡住。
你必须知道它们大概落到驱动哪层：

1. `v4l2_ioctl_ops`
2. `vb2_queue / vb2_ops`
3. 中断完成路径（驱动把 buffer 标 done）

---

## 2. Stage02 主流程对应驱动侧“谁在干活”

### 用户态：`VIDIOC_QUERYCAP`

驱动侧常见对应：

1. `vidioc_querycap`

你要理解：

1. 这里返回 capability，决定这个节点是不是 capture/m2m/streaming。
2. 你开错节点，后面一切流程都会错。

---

### 用户态：`VIDIOC_S_FMT / VIDIOC_G_FMT`

驱动侧常见对应：

1. `vidioc_try_fmt_vid_cap`
2. `vidioc_s_fmt_vid_cap`
3. `vidioc_g_fmt_vid_cap`

你要理解：

1. `S_FMT` 是请求，不保证完全按你给的值。
2. 真正后续内存布局要以 `G_FMT` 回读为准。

---

### 用户态：`VIDIOC_REQBUFS`

驱动侧常见对应（通过 vb2）：

1. `vb2_queue` 初始化后，触发 `vb2_ops.queue_setup`

你要理解：

1. 驱动在这里决定 buffer 数量、plane 数、每 plane 大小。
2. 所以你请求 8 个，驱动可能回你 4 个。

---

### 用户态：`VIDIOC_QBUF`

驱动侧常见对应（vb2）：

1. `vb2_ops.buf_prepare`
2. `vb2_ops.buf_queue`

你要理解：

1. `QBUF` 不是“立刻处理完”，而是把 buffer 交给驱动队列。
2. 什么时候真正完成，要等中断/任务完成再回到 `DQBUF`。

---

### 用户态：`VIDIOC_STREAMON`

驱动侧常见对应（vb2）：

1. `vb2_ops.start_streaming`

你要理解：

1. 这里通常做硬件启动、DMA启动、任务下发。
2. 如果这步失败，最常见是格式/缓冲/硬件状态不一致。

---

### 用户态：`VIDIOC_DQBUF`

驱动侧常见对应（vb2 + 中断路径）：

1. 硬件完成后，驱动在中断或下半部把 buffer 标记 done
2. 用户态 `DQBUF` 取出已完成 buffer

你要理解：

1. `DQBUF` 卡住通常是“上游没有完成事件”。
2. 根因可能在硬件、DMA、中断、队列饥饿，而不一定在用户态。

---

### 用户态：`VIDIOC_STREAMOFF`

驱动侧常见对应（vb2）：

1. `vb2_ops.stop_streaming`

你要理解：

1. 停流必须回收挂起 buffer，停止硬件任务。
2. cleanup 不完整会导致下一次 start 失败。

---

## 3. Stage02 两个策略在驱动视角怎么理解

### `drop-oldest`

用户态行为：队列满丢旧帧。

驱动视角含义：

1. 驱动侧仍然稳定产出完成 buffer。
2. 问题在“用户态消费策略”偏实时。

### `block`

用户态行为：队列满时阻塞生产者。

驱动视角含义：

1. 用户态可能变慢，进而影响回队节奏。
2. 回队慢会导致驱动可用 buffer 变少，极端时触发 timeout 风险。

---

## 4. 你现在只需要记住这张最小映射表

1. `QUERYCAP` -> `vidioc_querycap`
2. `S/G/TRY_FMT` -> `vidioc_*_fmt_*`
3. `REQBUFS` -> `vb2 queue_setup`
4. `QBUF` -> `vb2 buf_prepare/buf_queue`
5. `STREAMON` -> `vb2 start_streaming`
6. `DQBUF` -> `vb2 done queue + 用户态取出`
7. `STREAMOFF` -> `vb2 stop_streaming`

---

## 本阶段总结：通过这些例子你学到了什么

1. 你学会了把 ioctl 名字映射到驱动回调层。
2. 你知道了为什么 QBUF/DQBUF 本质是“队列与完成事件模型”。
3. 你开始具备“用户态日志异常 -> 可能驱动位置”的定位思路。
