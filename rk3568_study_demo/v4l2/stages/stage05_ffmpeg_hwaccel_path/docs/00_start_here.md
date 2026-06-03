# Stage05 从这里开始

## 0) 先确认环境职责

Stage05 的真实硬件验证优先在 RK 板执行。

| 环境 | 主要任务 | 验收结论 |
| --- | --- | --- |
| RK 板 | 跑 `h264_rkmpp/hevc_rkmpp`，做软件 vs RKMPP 对比 | 证明 RKMPP 是否可用 |
| VM | 学 FFmpeg hwaccel API 和后端选择 | 不证明 RK 板硬件路径 |

如果你要学习 V4L2 M2M ioctl/queue 逻辑，请回 Stage03/Stage06 在 VM 跑；如果你要验证 RK 板真实硬解，请继续本阶段。

## 1) 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev pkg-config
```

## 2) 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage05_ffmpeg_hwaccel_path
./build.sh all
```

## 3) 准备样本

放置：
- `samples/sample.mp4`

## 4) 运行

```bash
./scripts/run_all_stage05.sh
```

或仅跑性能定位（推荐先做这个）：

```bash
INPUT=./samples/sample.mp4 MAX_FRAMES=120 LOOPS=5 ./scripts/run_10_performance_diagnosis_playbook.sh
```

## 5) 验收入口

1. `docs/02_final_checklist.md`
2. `docs/04_experiment_matrix.md`
3. `enterprise_project/docs/09_enterprise_verification_guide.md`

报告开头必须写：

```text
environment=RK_BOARD
backend=rkmpp
hardware_proof=yes
what_this_proves=FFmpeg can use RKMPP decoder wrapper on this board
what_this_does_not_prove=A generic V4L2 M2M codec node exists on this board
```
