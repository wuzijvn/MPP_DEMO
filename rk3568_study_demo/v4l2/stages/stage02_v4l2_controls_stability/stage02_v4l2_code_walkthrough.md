# Stage02 代码精讲（逐模块教学版）

> 目标：你不仅会“跑起来”，还要能解释“为什么这么设计”。

## 1. 先看主入口

入口文件：  
- [`stage02_v4l2_controls_stability_main.cpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability/stage02_v4l2_controls_stability_main.cpp)

主流程很短，三步：
1. `parse_args` 解析参数
2. 打印最终配置
3. `run_stage02` 执行完整链路

这个设计是典型工程做法：入口只做编排，不塞业务细节。

## 2. 参数层（你每天最常改）

参数定义与校验：  
- [`stage02_v4l2_args.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability/stage02_v4l2_args.hpp)

关键点：
1. `--duration-sec`：稳定性测试首选，不建议只跑几百帧
2. `--queue-policy=drop-oldest|block`：背压策略核心开关
3. `--writer-delay-ms`：模拟慢消费，复现现实瓶颈
4. `--recover-on-timeout` + `--max-recoveries`：恢复策略实验开关
5. `--set-ctrl=KEY=VAL`：控制项联调入口

## 3. 类型层（理解数据是如何流动的）

类型定义：  
- [`stage02_v4l2_types.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability/stage02_v4l2_types.hpp)

你重点记住：
1. `FramePacket`：capture 线程传给 writer 线程的帧对象
2. `QueueState`：队列状态 + 背压统计（dropped/block）
3. `Stage2Stats`：所有指标的“证据池”

这三个结构决定了你后续是否能做出“可解释”的性能结论。

## 4. 控制项层（QUERYCTRL/G_CTRL/S_CTRL）

控制项模块：  
- [`stage02_v4l2_ctrls.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability/stage02_v4l2_ctrls.hpp)

流程：
1. `enumerate_controls`：优先 `NEXT_CTRL`，失败回退 legacy range
2. `make_ctrl_name_map`：把名字标准化成命令行友好形式
3. `apply_control_requests`：按 `before -> set -> after` 验证生效

为什么这很重要：
1. 你不是“盲调参数”，而是“可验证调参”
2. 你可以把控制项变化和稳定性指标变化关联起来

## 5. 采集核心层（线程 + 恢复 + 统计）

核心文件：  
- [`stage02_v4l2_capture.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability/stage02_v4l2_capture.hpp)

### 5.1 初始化链路

顺序仍是标准 V4L2 state machine：
1. `open`
2. `QUERYCAP`
3. `S_FMT/G_FMT`
4. `S_PARM`
5. `REQBUFS/QUERYBUF/MMAP`
6. 初始 `QBUF`
7. `STREAMON`

### 5.2 线程解耦

1. capture 线程：`select -> DQBUF -> 复制 -> 入队 -> QBUF`
2. writer 线程：从队列取帧，按 `dump_every` 节奏落盘

这样做的原因：
1. 采集线程尽量短路径，避免被慢写盘拖垮
2. 可独立观测背压：`dropped_oldest` 或 `blocked_waits`

### 5.3 背压策略

1. `drop-oldest`
- 队列满就丢旧帧
- 实时性更好，完整性更差

2. `block`
- 队列满就阻塞生产者
- 完整性更好，抖动风险更高

### 5.4 timeout 恢复

timeout 时（`select r==0`）：
1. 记录 timeout
2. 若允许恢复且未超次数：
   - `STREAMOFF`
   - 全量 `QBUF`
   - `STREAMON`
3. 恢复失败则退出

这条路径就是你岗位“错误检测与恢复机制”的入门版。

## 6. summary 怎么读（工作里最有用）

先看：
1. `duration/fps`
2. `dq_ok/dq_fail/qbuf_fail`
3. `recoveries_ok/fail`
4. `queue peak/dropped/block`
5. `host interval min/max/avg`

结论模板（你可以照抄）：
1. 现象：在 XXX 配置下出现 XXX 指标异常
2. 证据：日志中 A/B/C 指标分别为 ...
3. 根因判断：偏向采集侧/下游侧/恢复路径
4. 改进动作：调队列、调 writer、调超时与恢复策略

## 7. 你现在应该能回答的问题

1. 为什么要把写盘从采集线程拆出去？
2. `drop-oldest` 和 `block` 分别适合什么业务目标？
3. timeout 恢复为什么是 `STREAMOFF -> requeue -> STREAMON`？
4. 什么情况下控制项设置失败但可以继续采集？
5. 如何用一组 summary 指标证明你的结论？
