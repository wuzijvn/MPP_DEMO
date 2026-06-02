# Stage02 学习路径（配合详细注释版代码）

> 你已经说过“看代码但不知道重点”。
> 这份文档只做一件事：告诉你**按什么顺序看、每一步要回答什么问题**。

## 0) 先知道 Stage02 在练什么

Stage02 不是为了学“更多 ioctl 名字”，而是为了建立这 3 个能力：

1. 控制项能力：会枚举、会设置、会验证是否生效
2. 稳定性能力：会跑长时间采集并看 timeout/dq/qbuf/recovery 指标
3. 工程能力：会处理采集线程与慢消费线程的解耦（backpressure）

对应文件：
- `stage02_v4l2_args.hpp`
- `stage02_v4l2_ctrls.hpp`
- `stage02_v4l2_capture.hpp`

---

## 1) 第一遍：只看“入口参数怎么变成行为”

看文件：`stage02_v4l2_args.hpp`

你只回答这 5 个问题：

1. `--duration-sec` 和 `--frames` 谁优先？
2. `--queue-policy` 两个模式是啥？
3. `--recover-on-timeout` / `--max-recoveries` 控制了哪段逻辑？
4. `--set-ctrl=KEY=VAL` 的 KEY 支持什么写法？
5. 哪些参数在 parse 阶段会直接判错退出？

如果你这 5 个问题能口述清楚，说明你已经能“看懂命令行如何驱动程序行为”。

---

## 2) 第二遍：只看 controls（不要管采集）

看文件：`stage02_v4l2_ctrls.hpp`

你重点盯 4 个函数：

1. `enumerate_controls`
2. `make_ctrl_name_map`
3. `set_ctrl_value`
4. `apply_control_requests`

你要搞懂：

1. 为啥先走 `NEXT_CTRL`，再回退 legacy 范围扫描？
2. 为什么要 `before -> set -> after` 三段打印？
3. 为什么 `set` 失败不一定要中断整条采集链路？

---

## 3) 第三遍：只看采集主循环的状态机

看文件：`stage02_v4l2_capture.hpp`

你可以把 `run_stage02` 当成 10 步：

1. open
2. querycap
3. controls
4. S_FMT/G_FMT
5. S_PARM
6. REQBUFS/QUERYBUF/MMAP
7. 初始 QBUF
8. 分支A（带 writer）
9. 分支B（纯采集）
10. cleanup

你最该盯的是第 8 步，因为这是工作里最常见的复杂场景。

---

## 4) 第四遍：只看“背压策略”

在 `stage02_v4l2_capture.hpp` 里找这段：

- `if (qs.policy == "drop-oldest") ... else ...`

然后回答：

1. `drop-oldest` 的业务目标是什么？
2. `block` 的业务目标是什么？
3. 你怎么从 summary 判断目前是“丢帧多”还是“阻塞多”？

对应指标：

1. `dropped_oldest`
2. `blocked_waits`
3. `peak_depth`

---

## 5) 第五遍：只看 timeout 恢复链路

在 `stage02_v4l2_capture.hpp` 里找：

1. `select timeout` 分支
2. `restart_stream`

你要理解的不是“它写了啥”，而是“为什么这样写”：

1. 为什么先 `STREAMOFF`，再 `requeue_all_buffers`，再 `STREAMON`？
2. 为什么要 `max_recoveries` 上限？
3. 恢复成功后继续跑，失败后退出，背后策略是什么？

---

## 6) 第六遍：只看清理路径（面试最爱问）

看 `cleanup:` 段，回答：

1. 为什么 writer 线程要在 cleanup 里再做一次 stop+broadcast 防御收尾？
2. 为什么 `STREAMOFF` 失败也继续 munmap/close？
3. 如果清理路径不完整，线上可能出现什么问题？

---

## 7) 你现在最该做的实践顺序

1. 先跑 `--list-ctrls`
2. 再跑 `--duration-sec=60 --no-save`（纯采集基线）
3. 再跑 `--duration-sec=60 --queue-depth=8 --queue-policy=drop-oldest --writer-delay-ms=20`
4. 再跑 `--duration-sec=60 --queue-depth=8 --queue-policy=block --writer-delay-ms=20`
5. 对比 summary 的 `dropped_oldest` 和 `blocked_waits`

---

## 本阶段总结：通过这些例子你学到了什么

1. 你学会了把“命令行参数”映射到“线程与状态机行为”。
2. 你学会了用可量化指标判断稳定性，而不是只看“有没有画面”。
3. 你学会了两种背压策略的业务取舍（实时性 vs 完整性）。
4. 你学会了 timeout 恢复链路的基本工程写法。
5. 你学会了为什么清理路径和异常路径在媒体程序里同等重要。
