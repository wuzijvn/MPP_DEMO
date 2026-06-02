# Stage05 运行主指南

## 按顺序执行

```bash
./build.sh all
./scripts/run_01_list_hwaccel_and_decoders.sh
./scripts/run_02_create_hwdevice_context.sh
./scripts/run_03_get_hw_pixel_format.sh
./scripts/run_04_hwframe_vs_swframe.sh INPUT=./samples/sample.mp4
./scripts/run_05_hwdownload_transfer.sh INPUT=./samples/sample.mp4
./scripts/run_06_drm_prime_frame_note.sh
./scripts/run_07_hwaccel_benchmark_commands.sh
./scripts/run_08_fallback_detection_checklist.sh
./scripts/run_09_enterprise_ffmpeg_hwaccel_service.sh
INPUT=./samples/sample.mp4 MAX_FRAMES=120 LOOPS=5 ./scripts/run_10_performance_diagnosis_playbook.sh
```

## 企业项目故障矩阵

```bash
cd enterprise_project
./scripts/run_09_enterprise_fault_matrix.sh
```

## 结果解读

1. 先看 `verdict`，不要先猜 frame 行：
   - `HARDWARE_FRAME_CONFIRMED`：显式 hwdevice 模式下确认硬件帧。
   - `HARDWARE_DECODE_WRAPPER_OUTPUT`：`rkmpp` 硬解 + CPU可见帧输出（不是软解回退）。
   - `SOFTWARE_FALLBACK`：软解回退。
   - `UNKNOWN_NEED_MORE_EVIDENCE`：证据不足，需补日志/对照实验。
2. 若 `04` 在显式 `HW_TYPE=...` 模式中出现 `hw_frames=0` 且 `sw_fallback_frames>0`：优先怀疑 fallback。
3. 若企业项目 `result=FAIL` 且 reason=“no hardware frame observed”：说明门禁检测到“假硬解”。
4. 若 `hw_transfer_fail` 增长：排查硬件帧到 CPU 回拷链路与后端能力。
5. `run_10` 的 `summary.csv` 是性能定位第一证据：先看 `verdict` 与 `sw_fallback_frames`，再看 `cpu_pct` 与 `real_sec`。
