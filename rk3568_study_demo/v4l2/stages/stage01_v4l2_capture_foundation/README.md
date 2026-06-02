# Stage 01: V4L2 Capture Foundation

该目录是你 V4L2 学习路线的第一阶段，目标是：
1. 连续采集 300 帧
2. 输出关键统计（fps/timeout/dq失败/bytesused分布）
3. 掌握请求值 vs 生效值（S_FMT/G_FMT、S_PARM/G_PARM）
4. 完成 3 个故障注入实验

## 目录说明

- `stage01_v4l2_capture_main.cpp`：主入口，流程编排
- `stage01_v4l2_capture.hpp`：V4L2 采集状态机
- `stage01_v4l2_args.hpp`：参数解析与校验
- `stage01_v4l2_stats.hpp`：统计与日志解释
- `stage01_v4l2_image.hpp`：YUYV -> PPM
- `stage01_v4l2_common.hpp`：通用工具
- `stage01_v4l2_types.hpp`：结构体定义
- `stage01_v4l2_capture_guide.md`：阶段任务手册
- `stage01_v4l2_code_walkthrough.md`：代码精讲
- `stage01_v4l2_deepening_notes.md`：岗位导向深化点
- `stage01_experiment_matrix.sh`：矩阵实验一键脚本（日志+CSV+报告）
- `stage01_5_runbook.md`：Stage01.5 两天强化执行清单
- `stage01_5_conclusion_template.md`：Stage01.5 结论表模板（直接填写）
- `stage01_5_run_all.sh`：Stage01.5 一键执行器（按 A~E 自动跑）
- `stage01_5_extract_metrics.sh`：从单个日志提取关键指标（pretty/csv）

## 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation
./build.sh stage01_v4l2_capture_main
```

## 运行

先说明（非常重要）：
- 你的机器上可直接跑通的 UVC 摄像头节点是 `/dev/video10`（`/dev/video11` 也可能是同一设备的另一路）。
- `/dev/video0~9` 多数是 rkisp 节点，不满足本 Stage01 对 `VIDEO_CAPTURE + STREAMING` 的检查条件，会报错退出。

```bash
./bin/stage01_v4l2_capture_main /dev/video10 640 480 ../../../artifacts/s01_raw.yuyv ../../../artifacts/s01_view.ppm 300
```

可选高级参数（岗位建议尽快用）：

```bash
./bin/stage01_v4l2_capture_main /dev/video10 1280 720 ../../../artifacts/s01_raw.yuyv ../../../artifacts/s01_view.ppm 300 --pixfmt=YUYV --dump-formats --trace-csv=../../../artifacts/s01_trace.csv --log-every=50
```

## 故障注入

```bash
./bin/stage01_v4l2_capture_main --inject=bad-node --no-save
./bin/stage01_v4l2_capture_main /dev/video10 640 480 badfmt.yuyv badfmt.ppm 30 --inject=bad-fmt
./bin/stage01_v4l2_capture_main /dev/video10 640 480 ../../../artifacts/skip.yuyv ../../../artifacts/skip.ppm 300 --inject=skip-requeue --inject-frame=30
```

## 一键矩阵实验

```bash
./stage01_experiment_matrix.sh --dev=/dev/video10 --build
```

## Stage01.5 一键强化（推荐）

```bash
./stage01_5_run_all.sh --dev=/dev/video10 --build
```

输出目录（每次不同时间戳）：
- `outputs_stage01_5/stage01_5_YYYYmmdd_HHMMSS/logs/*.log`
- `outputs_stage01_5/stage01_5_YYYYmmdd_HHMMSS/summary.csv`
- `outputs_stage01_5/stage01_5_YYYYmmdd_HHMMSS/summary.md`

如果你只想抽取某一个日志的关键字段：

```bash
./stage01_5_extract_metrics.sh outputs_stage01_5/stage01_5_YYYYmmdd_HHMMSS/logs/A_baseline_640x480_yuyv_buf4.log
./stage01_5_extract_metrics.sh --csv --label=my_case outputs_stage01_5/stage01_5_YYYYmmdd_HHMMSS/logs/A_baseline_640x480_yuyv_buf4.log
```
