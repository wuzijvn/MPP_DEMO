# Week2 扩展学习计划（并发与工程排障）

> 目标：从“会写多线程”升级到“能定位并发问题、能解释性能差异、能给出稳定修复方案”。

## 本周代码清单

1. `week_01_多线程.cpp`
2. `week_02_锁_竞态与互斥.cpp`
3. `week_02_day2_队列背压.cpp`
4. `week_02_day3_超时与优雅退出.cpp`
5. `week_02_day4_锁顺序与死锁规避.cpp`
6. `week_02_day5_原子操作与内存序.cpp`

---

## Day2：队列容量与背压

代码：`week_02_day2_队列背压.cpp`

编译：

```bash
g++ -std=c++11 -O2 -pthread week_02_day2_队列背压.cpp -o week_02_day2_backpressure
```

运行（扫描队列容量 1/2/4/8）：

```bash
./week_02_day2_backpressure 120 8 14 6 0 0
./week_02_day2_backpressure 120 8 6 14 0 0
```

关注指标：

1. `raw_push_wait` / `pkt_push_wait`：上游被背压程度
2. `avg_e2e`：业务体验延迟
3. `peak_depth`：是否顶满容量

---

## Day3：超时等待与优雅退出

代码：`week_02_day3_超时与优雅退出.cpp`

编译：

```bash
g++ -std=c++11 -O2 -pthread week_02_day3_超时与优雅退出.cpp -o week_02_day3_shutdown
```

运行：

```bash
./week_02_day3_shutdown 3000 80
```

关注指标：

1. `stop_to_exit`：停机到线程退出时延
2. `timeout_hits`：超时次数
3. `produced == consumed`：是否完整消费

结论目标：

1. 明确 `stop + close + join` 是推荐停机协议
2. 认识到“超时不等于异常”，要结合流状态判断

---

## Day4：锁顺序与死锁规避

代码：`week_02_day4_锁顺序与死锁规避.cpp`

编译：

```bash
g++ -std=c++11 -O2 -pthread week_02_day4_锁顺序与死锁规避.cpp -o week_02_day4_lock_order
```

运行：

```bash
./week_02_day4_lock_order 1500
```

关注指标：

1. `collision_rate`：锁顺序冲突率
2. `ok_ops`：有效吞吐

结论目标：

1. 明确“统一锁序”能破坏循环等待条件
2. 知道地址排序只是手段，工程上更推荐 lock rank 编号

---

## Day5：原子操作与内存序

代码：`week_02_day5_原子操作与内存序.cpp`

编译：

```bash
g++ -std=c++11 -O2 -pthread week_02_day5_原子操作与内存序.cpp -o week_02_day5_atomic
```

运行：

```bash
./week_02_day5_atomic 8 400000 200000
```

关注指标：

1. `MUTEX / ATOMIC_RELAXED / ATOMIC_SEQ_CST` 吞吐对比
2. 发布-订阅实验 `errors` 是否为 0

结论目标：

1. 统计计数类优先考虑 `atomic + relaxed`
2. 发布数据用 `release/acquire`
3. 多变量一致性依然要锁

---

## Day6：工具化定位（TSAN + perf）

TSAN（线程竞态）：

```bash
g++ -std=c++11 -O1 -g -fsanitize=thread -fno-omit-frame-pointer -pthread week_02_锁_竞态与互斥.cpp -o week_02_lock_lab_tsan
./week_02_lock_lab_tsan
```

perf（性能计数）：

```bash
perf stat -e context-switches,cpu-migrations,cache-misses ./week_02_lock_lab 8 500000 3
```

目标：

1. 能解释一条 TSAN 报告
2. 能把性能差异归因到锁竞争/调度切换/缓存行为

---

## Day7：总结输出（必须落地）

建议产物：

1. `week2_report.md`
2. 锁选型表（mutex/rwlock/atomic/cond）
3. 退出协议模板（stop/close/join 时序图）
4. 排障 checklist（死锁、竞态、背压、超时）

建议报告结构：

1. 实验环境（CPU、系统、编译参数）
2. 每个实验的参数与结果表
3. 关键现象与原因解释
4. 可迁移到音视频项目的结论

