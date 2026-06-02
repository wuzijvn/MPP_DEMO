# Stage02 实战教材：V4L2 Controls + 稳定性工程

> 目标周期：建议 7~14 天  
> 对应岗位能力：控制链路调试、采集稳定性、异常恢复、性能观测

## 1. 你会学到什么

1. `QUERYCTRL/G_CTRL/S_CTRL` 的真实使用方法  
2. 为什么采集线程与写盘线程要分离  
3. backpressure 如何通过队列策略反映到指标  
4. timeout 场景下如何做“有限次数自动恢复”  
5. 如何产出可复盘的稳定性报告

## 2. 本阶段程序新增了什么能力

1. 列控制项：`--list-ctrls`
2. 设置控制项：`--set-ctrl=KEY=VAL`（可重复）
3. 时间驱动稳定性跑测：`--duration-sec`
4. 线程解耦：capture + writer
5. 队列策略：
   - `drop-oldest`
   - `block`
6. 慢写盘模拟：`--writer-delay-ms`
7. 采集恢复：`--recover-on-timeout=1 --max-recoveries=N`

## 3. 关键参数说明（工作高频）

1. `--duration-sec=N`
   - 稳定性测试优先用它，避免只看短时帧数。
2. `--queue-depth=N`
   - 队列越小越容易触发背压；越大越吃内存和时延。
3. `--queue-policy=drop-oldest|block`
   - `drop-oldest`：保吞吐，牺牲完整性  
   - `block`：保完整性，可能卡住采集侧
4. `--writer-delay-ms=N`
   - 人工制造“下游慢消费”，验证系统韧性。
5. `--dump-every=N`
   - 每 N 帧落一次 raw，避免 I/O 成为主要瓶颈。
6. `--recover-on-timeout=1`
   - 超时后尝试 `STREAMOFF + requeue + STREAMON`。

## 4. 必跑实验（建议顺序）

### 实验 A：控制项枚举

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video10 640 480 --list-ctrls
```

输出关注：
1. id/name/type/min/max/step/default/flags
2. 找到你设备支持的亮度、对比度、曝光相关控件

### 实验 B：控制项设置前后对比

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video10 640 480 \
  --set-ctrl=brightness=128 \
  --set-ctrl=contrast=64 \
  --duration-sec=30 \
  --dump-every=30 \
  --out-dir=../../../artifacts/stage02_ctrl_test
```

输出关注：
1. 控制项 before/after 是否生效
2. 生效后 fps、error_flag、timeout 是否变化

### 实验 C：慢写盘背压压测（drop-oldest）

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video10 640 480 \
  --duration-sec=50 \
  --queue-depth=8 \
  --queue-policy=drop-oldest \
  --writer-delay-ms=20 \
  --dump-every=60 \
  --out-dir=../../../artifacts/stage02_bp_drop
```

预期：
1. `dropped_oldest` 增长
2. `peak_depth` 接近队列上限
3. 采集吞吐通常更稳定

### 实验 D：慢写盘背压压测（block）

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video10 640 480 \
  --duration-sec=120 \
  --queue-depth=8 \
  --queue-policy=block \
  --writer-delay-ms=20 \
  --dump-every=60 \
  --out-dir=../../../artifacts/stage02_bp_block
```

预期：
1. `blocked_waits` 增长
2. `host interval` 抖动可能上升
3. 和 drop-oldest 做对比分析

### 实验 E：恢复策略验证

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video0 640 480 \
  --duration-sec=300 \
  --timeout-ms=500 \
  --recover-on-timeout=1 \
  --max-recoveries=3 \
  --out-dir=../../../artifacts/stage02_recover
```

输出关注：
1. `recoveries_attempted/ok/fail`
2. 恢复后是否继续稳定采集

## 5. 指标解释（总结区）

1. `error_flag_frames`
   - 驱动标记了 buffer error 的帧数，优先排查链路异常。
2. `zero_bytes_frames`
   - payload 为 0 的帧，常见于异常/恢复边界。
3. `dropped_oldest`
   - 下游慢时主动丢旧帧次数，反映“保实时”策略代价。
4. `blocked_waits`
   - producer 被队列挤压阻塞次数，反映“保完整”代价。
5. `host interval min/max/avg`
   - 用户态 dq 节奏抖动，能反推系统负载和调度影响。

## 6. 常见坑

1. 忽略控件 flags 直接 S_CTRL，导致“看似成功但不生效”。
2. 长稳测试仍按 300 帧跑，根本看不出问题。
3. writer 同线程做重 I/O，导致采集抖动。
4. 没有队列策略实验，无法解释为何丢帧/卡顿。
5. timeout 后直接退出，不做恢复路径验证。

## 7. 每日打卡最小产出

1. 一条命令
2. 一份 summary 指标
3. 一句话根因判断
4. 一条改进动作
