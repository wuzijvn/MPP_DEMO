# Stage 02: V4L2 Controls + Stability Engineering

本阶段目标：
1. 学会 control 枚举/读写（`QUERYCTRL/G_CTRL/S_CTRL`）
2. 学会采集线程与写盘线程解耦（队列/backpressure）
3. 完成 10~30 分钟稳定性跑测并输出量化指标

## 目录说明

- `stage02_v4l2_controls_stability_main.cpp`：主入口
- `stage02_v4l2_args.hpp`：参数解析
- `stage02_v4l2_ctrls.hpp`：控制项枚举/读写
- `stage02_v4l2_capture.hpp`：采集主循环 + writer线程 + 恢复策略
- `stage02_v4l2_common.hpp`：xioctl/fourcc/计时
- `stage02_v4l2_types.hpp`：配置和统计结构
- `stage02_v4l2_controls_stability_guide.md`：阶段手册
- `stage02_v4l2_code_walkthrough.md`：代码精讲（教学注释配套）
- `stage02_v4l2_learning_path.md`：按问题驱动的阅读路径（先看哪里、回答什么）
- `stage02_driver_mapping_foundation.md`：驱动渗透01（ioctl 到驱动回调映射）
- `stage02_vb2_lifecycle_pseudocode.md`：驱动渗透02（vb2 生命周期最小伪代码）
- `stage02_driver_debug_checklist.md`：驱动渗透03（用户态现象到驱动排障清单）
- `stage02_collect_evidence.sh`：一键采集用户态 + v4l2 + dmesg 证据包

## 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage02_v4l2_controls_stability
./build.sh stage02_v4l2_controls_stability_main
```

## 快速运行

1) 列出控制项：

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video0 640 480 --list-ctrls
```

2) 跑 60 秒稳定性：

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video0 640 480 --duration-sec=60 --dump-every=120 --out-dir=../../../artifacts/stage02_run
```

3) 模拟慢 writer + 小队列压测：

```bash
./bin/stage02_v4l2_controls_stability_main /dev/video0 640 480 --duration-sec=120 --queue-depth=8 --queue-policy=drop-oldest --writer-delay-ms=20 --dump-every=60 --out-dir=../../../artifacts/stage02_bp
```

4) 一键采集证据包（推荐）：

```bash
./stage02_collect_evidence.sh --dev=/dev/video10 --duration-sec=30 -- --queue-depth=8 --queue-policy=block --writer-delay-ms=20
```

## 推荐关注指标

1. `select timeout`
2. `dq_ok / dq_fail / dq_eagain`
3. `error_flag_frames / zero_bytes_frames`
4. `recoveries_ok / recoveries_fail`
5. `queue peak_depth / dropped_oldest / blocked_waits`
6. `host interval min/max/avg`
