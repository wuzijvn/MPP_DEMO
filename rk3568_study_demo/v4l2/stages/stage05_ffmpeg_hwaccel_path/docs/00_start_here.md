# Stage05 从这里开始

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
