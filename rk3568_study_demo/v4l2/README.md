# V4L2 Workspace

这个目录用于放置所有 V4L2 相关代码与脚本。

## 当前 codec 学习路线调整

从 2026-06-02 起，codec 相关学习按“双环境路线”执行：

1. `VM / x86 Linux`：学习 V4L2 M2M 通用 ioctl、OUTPUT/CAPTURE 双队列、QBUF/DQBUF、SOURCE_CHANGE、EOS/drain、timeout debug。VM 上的虚拟 M2M 节点只证明状态机和用户态逻辑，不证明 RK 板真实 VPU 硬解。
2. `RK 板 / RK3568`：验证真实硬件路径，当前主线走 RKMPP，例如 FFmpeg `h264_rkmpp` / `hevc_rkmpp`。如果 RK 板没有 V4L2 M2M codec 字符设备，不要强行对 rkisp/camera 节点跑 codec ioctl。

详细路线入口：

```bash
less /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_dual_environment_codec_route.md
```

## 目录结构（按阶段组织）

- `stages/`
  - 阶段化代码与文档索引
  - 入口文档：`stages/README.md`
- `stages/stage01_v4l2_capture_foundation/`
  - 阶段1：V4L2 采集基础（连续采集、统计、故障注入）
- `stages/stage02_v4l2_controls_stability/`
  - 阶段2：V4L2 控制项与稳定性工程（controls、线程队列、恢复策略）
- `stages/stage03_v4l2_m2m_codec_foundation/`
  - VM 优先：V4L2 M2M 双队列和 ioctl 状态机训练
- `stages/stage05_ffmpeg_hwaccel_path/`
  - RK 板优先：FFmpeg `h264_rkmpp/hevc_rkmpp` 真实硬解验证
- `stages/stage06_v4l2_m2m_deep_dive/`
  - VM 深化 V4L2 M2M 状态机，RK 板做 reality check 和 RKMPP 适配边界
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
