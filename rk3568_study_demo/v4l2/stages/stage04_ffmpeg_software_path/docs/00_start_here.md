# Stage04 从这里开始

## 1) 准备依赖

```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev pkg-config
```

## 2) 进入目录并构建

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage04_ffmpeg_software_path
./build.sh all
```

## 3) 准备样本

把测试输入放到：
- `samples/sample.mp4`

## 4) 跑全套

```bash
./scripts/run_all_stage04.sh
```

## 5) 验收入口

1. 对照 `docs/02_final_checklist.md`
2. 对照 `docs/04_experiment_matrix.md`
3. 企业级项目验证：`enterprise_project/docs/09_enterprise_verification_guide.md`
