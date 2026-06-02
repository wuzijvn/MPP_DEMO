# Stage01 实战教材：V4L2 连续采集 300 帧 + 统计 + 故障注入

> 目标周期：2026-05-01 ~ 2026-05-14  
> 对应岗位能力：V4L2 用户态链路可观测性、稳定性、基础错误定位

## 1. 你将学到什么（必须掌握）

1. 标准 V4L2 streaming 状态机：
   `open -> QUERYCAP -> S_FMT/G_FMT -> S_PARM/G_PARM -> REQBUFS -> QUERYBUF -> MMAP -> QBUF -> STREAMON -> (select + DQBUF + QBUF)*N -> STREAMOFF`
2. 为什么要看 `G_FMT/G_PARM` 回读，而不是只信请求值。
3. 如何建立“可回归”的量化指标：`fps/timeout/dq失败/bytesused分布`。
4. 如何做故障注入，并通过日志定位问题。

## 2. 代码位置（已按模块拆分）

主入口（只保留流程骨架）：
- [`stage01_v4l2_capture_main.cpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture_main.cpp)

参数与配置：
- [`stage01_v4l2_args.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_args.hpp)

V4L2 采集主流程：
- [`stage01_v4l2_capture.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture.hpp)

统计结构与打印：
- [`stage01_v4l2_stats.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_stats.hpp)

图像转换与保存：
- [`stage01_v4l2_image.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_image.hpp)

通用小工具：
- [`stage01_v4l2_common.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_common.hpp)

类型定义：
- [`stage01_v4l2_types.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_types.hpp)

## 3. 编译与基础运行

### 3.1 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation
./build.sh stage01_v4l2_capture_main
```

### 3.2 默认运行（300 帧）

```bash
./bin/stage01_v4l2_capture_main
```

默认等价于：
- device: `/dev/video10`
- request: `640x480` + `pixfmt=YUYV`
- target frames: `300`
- warmup: `3`
- request fps: `30`
- timeout: `2000ms`
- reqbufs: `4`
- inject: `none`

节点提醒（结合你当前机器）：
1. `/dev/video10` 是可直接用于本阶段的 UVC capture 节点（已实测可跑）。
2. `/dev/video0~9` 多为 rkisp 节点，当前本程序会判定为“非 capture+streaming”而退出。
3. 如果你改用别的机器，先跑一次 `./bin/stage01_v4l2_capture_main --dump-formats --no-save` 或 `v4l2-ctl --list-devices` 再定节点。

### 3.3 指定参数运行

```bash
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 ../../../artifacts/s1_raw.yuyv ../../../artifacts/s1_view.ppm 300 --warmup=5 --fps=30 --timeout-ms=2000 --req-bufs=4
```

## 4. 参数说明（工作中常用）

位置参数：
1. `dev`：视频节点
2. `width`
3. `height`
4. `raw_out`
5. `ppm_out`
6. `frames`

可选参数：
1. `--warmup=N`：前 N 帧不作为预览保存依据（自动曝光收敛）
2. `--fps=N`：请求帧率（驱动可能不严格执行）
3. `--pixfmt=FOURCC`：请求像素格式（如 `YUYV/NV12/MJPG`）
4. `--timeout-ms=N`：`select` 等待超时
5. `--req-bufs=N`：申请 MMAP 缓冲数
6. `--dump-formats`：打印设备支持的 format/size/fps 枚举
7. `--inject=none|bad-node|bad-fmt|skip-requeue`
8. `--inject-frame=N`：`skip-requeue` 生效起始帧
9. `--no-save`：仅跑统计，不落盘 raw/ppm
10. `--trace-csv=PATH`：输出逐帧 trace CSV（sequence/flags/timestamp/interval）
11. `--log-every=N`：逐帧日志打印节奏控制

## 5. 输出怎么读（重点）

### 5.1 “请求值 vs 生效值”

程序会打印三组格式信息：
1. `request`
2. `active(S_FMT)`
3. `readback(G_FMT)`

这三组不一定相同，常见原因：
1. 驱动就近调整分辨率
2. 驱动改了 field
3. bytesperline/sizeimage 被对齐扩展
4. 某些像素格式不支持，会被替换或直接失败

帧率同理：
1. `request fps`
2. `active(S_PARM)`
3. `readback(G_PARM)`

注意：UVC 设备常出现“请求 30fps，但实际回读不是 30”的情况，这很正常。

额外建议：
1. 先 `--dump-formats` 看设备能力，再选 `--pixfmt` 与分辨率组合。
2. 先看 `TRY_FMT` 输出，再看 `S_FMT/G_FMT` 最终结果。

### 5.2 统计指标

`capture summary` 里看这几行：
1. `fps(actual dq_ok / duration)`：实际吞吐
2. `select timeout`：等待超时次数
3. `dqbuf fail/eagain`：取帧失败分类
4. `qbuf requeue skipped/fail`：回队异常
5. `bytesused` 分布：观察帧大小是否稳定
6. `sequence observed_gap_frames`：粗看是否有帧序跳跃
7. `payload shape`：zero bytes 与 bytes_over_sizeimage 计数
8. `dq host interval(ms)`：用户态取帧抖动范围（min/max/avg）
9. `v4l2 timestamp interval(ms)`：驱动时间戳节奏（需结合 flags 解读）
10. `buffer flags distribution`：观察 `ERROR/TS_MONOTONIC/SRC_*` 等行为

### 5.3 trace CSV 怎么用

示例：

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 ../../../artifacts/t_raw.yuyv ../../../artifacts/t_view.ppm 300 --trace-csv=../../../artifacts/t_trace.csv --log-every=30
```

CSV 列含义（核心列）：
1. `frame_no`：本次运行的第几帧
2. `sequence`：驱动 sequence
3. `bytesused`：payload 大小
4. `flags_text`：buffer flags 可读串
5. `host_delta_ms`：相邻 DQ 的用户态间隔
6. `v4l2_delta_ms`：相邻驱动 timestamp 间隔

看法：
1. `host_delta_ms` 抖动大 -> 用户态调度或系统负载问题可能性高
2. `v4l2_delta_ms` 抖动大 -> 传感器/驱动节奏异常概率高
3. `flags` 若出现 `ERROR` -> 优先走链路错误排查

## 6. 三个故障注入实验（必须做）

> 每个实验都要记录：命令、关键日志、根因、修复。

### 6.1 实验 A：错误节点（bad-node）

```bash
./bin/stage01_v4l2_capture_main --inject=bad-node
```

预期：
1. `open video device failed`
2. 程序快速失败退出

根因：
1. 设备节点不存在，初始化第一步失败

修复：
1. 用真实节点替换，或先用 `v4l2-ctl --list-devices` 确认

### 6.2 实验 B：错误格式（bad-fmt）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 badfmt.yuyv badfmt.ppm 30 --inject=bad-fmt
```

预期：
1. `VIDIOC_S_FMT` 报错（常见 `EINVAL`）
2. 提示 `hint: if inject=bad-fmt this failure is expected`

根因：
1. 请求的 fourcc 不被驱动支持

修复：
1. 回退到可支持格式（本例默认 YUYV）
2. 实战里应先 `ENUM_FMT/ENUM_FRAMESIZES` 再协商

### 6.3 实验 C：漏回队（skip-requeue）

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 ../../../artifacts/skip.yuyv ../../../artifacts/skip.ppm 300 --inject=skip-requeue --inject-frame=30
```

预期：
1. 前期正常 DQ
2. 到触发点后不断打印 `[inject] skip requeue ...`
3. 最终出现 `select timeout` 或 DQ 异常
4. summary 中 `requeue_skipped` 非 0

根因：
1. DQ 后不 Q，驱动侧空闲 buffer 被耗尽

修复：
1. 保证每个成功 DQ 的 buffer 最终都回队
2. 异常分支也要考虑回队或统一清理

## 7. 常见坑（工作高频）

1. 只看 `S_FMT` 请求，不看 `G_FMT` 回读：
   结果是后面按错误尺寸/stride 处理，图像错位。

2. 假设 `bytesused == width*height*2` 永远成立：
   某些驱动会因为 padding 或格式变化导致偏差。

3. `select` 超时时调用 `perror("select")`：
   `r==0` 不是 errno 错误，不能直接 perror。

4. 错误路径提前 return，没 `STREAMOFF/munmap`：
   demo 里可能“看起来没事”，工程里会积累隐患。

5. 忽视 `V4L2_CAP_DEVICE_CAPS`：
   capability 判断可能误判。

6. YUYV 宽度给奇数：
   4:2:2 两像素一组，转换器常要求偶数宽。

7. 忽略 stride（bytesperline）：
   直接按 `width*2` 读 YUYV，可能出现图像错位；本阶段已支持 stride 感知导出。

## 8. 建议你这 2 周的打卡模板

每天最少产出四件：
1. 可运行命令（含参数）
2. 关键日志片段
3. 指标表（fps/timeout/dq_fail/bytesused）
4. 复盘一句：今天定位了什么问题

建议你建立一个 `dayXX_report.md`，固定记录格式：

```md
## 环境
- 日期:
- 设备节点:
- 分辨率:
- 帧率请求:

## 命令
...

## 结果
- fps:
- timeout:
- dq_fail:
- bytesused(min/max/avg):

## 问题与结论
- 现象:
- 根因:
- 修复:
```

## 9. 下一步预告（阶段2）

完成本阶段后，下一步直接进入：
1. 多线程采集+写盘分离
2. 控制项 `QUERYCTRL/G_CTRL/S_CTRL`
3. 10 分钟稳定性跑测与基线输出

## 10. 一键矩阵实验（建议每天跑一次）

```bash
./stage01_experiment_matrix.sh --dev=/dev/video10 --build
```

输出：
1. `outputs/stage01_matrix_<timestamp>/report.md`：汇总表
2. `outputs/.../logs/*.log`：每个 case 原始日志
3. `outputs/.../traces/*.csv`：逐帧 trace
