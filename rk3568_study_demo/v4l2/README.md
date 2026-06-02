# V4L2 Workspace

这个目录用于放置所有 V4L2 相关代码与脚本。

## 目录结构（按阶段组织）

- `stages/`
  - 阶段化代码与文档索引
  - 入口文档：`stages/README.md`
- `stages/stage01_v4l2_capture_foundation/`
  - 阶段1：V4L2 采集基础（连续采集、统计、故障注入）
- `stages/stage02_v4l2_controls_stability/`
  - 阶段2：V4L2 控制项与稳定性工程（controls、线程队列、恢复策略）
- `learning_plan/`
  - 路线与打卡计划文档（通用）

## 快速开始（阶段1）

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation
./build.sh stage01_v4l2_capture_main
./bin/stage01_v4l2_capture_main /dev/video0 640 480 ../../../artifacts/s01_raw.yuyv ../../../artifacts/s01_view.ppm 300
```

## 阶段文档

- 阶段索引：
  - `stages/README.md`
- 阶段说明：
  - `stages/stage01_v4l2_capture_foundation/README.md`
- 阶段手册：
  - `stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture_guide.md`
- 代码精讲：
  - `stages/stage01_v4l2_capture_foundation/stage01_v4l2_code_walkthrough.md`

## 其他学习资料

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2
ls learning_plan
```
