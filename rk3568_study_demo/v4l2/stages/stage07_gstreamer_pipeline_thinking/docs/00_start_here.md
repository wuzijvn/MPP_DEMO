# Stage07 Start Here

推荐阅读顺序：

1. `README.md`：先看阶段目标、Demo Map、验收标准。
2. `docs/01_master_study_manual.md`：系统学习 GStreamer pipeline 思维。
3. `src/01_*` 到 `src/07_*`：按编号读代码并运行脚本。
4. `docs/02_code_walkthrough.md`：对照源码理解数据流和错误路径。
5. `enterprise_project/README.md`：进入企业级诊断服务。
6. `docs/03_experiment_matrix.md`：按矩阵做实验。
7. `docs/04_driver_shadow_note.md`：把 GStreamer 现象映射到驱动/后端。
8. `docs/05_acceptance_checklist.md`：做最终自检。

构建和运行：

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage07_gstreamer_pipeline_thinking
./build.sh all-with-enterprise
./scripts/run_all_stage07.sh
./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh
```

边界提醒：

- 本阶段默认使用 `videotestsrc`，证明 pipeline/caps/queue/debug 能力。
- 硬解证明需要真实压缩码流和后端/驱动证据；不要把插件存在当作硬件 proof。
