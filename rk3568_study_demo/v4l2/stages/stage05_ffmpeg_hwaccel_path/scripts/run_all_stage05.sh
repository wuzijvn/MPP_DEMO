#!/usr/bin/env bash
set -euo pipefail
LOG_DIR="${1:-./logs/run_all_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${LOG_DIR}"
echo "[stage05 run_all] log_dir=${LOG_DIR}"
./build.sh all | tee "${LOG_DIR}/build.log" || true
./scripts/run_01_list_hwaccel_and_decoders.sh | tee "${LOG_DIR}/01_list_hwaccel_and_decoders.log" || true
./scripts/run_02_create_hwdevice_context.sh | tee "${LOG_DIR}/02_create_hwdevice_context.log" || true
./scripts/run_03_get_hw_pixel_format.sh | tee "${LOG_DIR}/03_get_hw_pixel_format.log" || true
./scripts/run_04_hwframe_vs_swframe.sh | tee "${LOG_DIR}/04_hwframe_vs_swframe.log" || true
./scripts/run_05_hwdownload_transfer.sh | tee "${LOG_DIR}/05_hwdownload_transfer.log" || true
./scripts/run_06_drm_prime_frame_note.sh | tee "${LOG_DIR}/06_drm_prime_frame_note.log" || true
./scripts/run_07_hwaccel_benchmark_commands.sh | tee "${LOG_DIR}/07_hwaccel_benchmark_commands.log" || true
./scripts/run_08_fallback_detection_checklist.sh | tee "${LOG_DIR}/08_fallback_detection_checklist.log" || true
./scripts/run_09_enterprise_ffmpeg_hwaccel_service.sh | tee "${LOG_DIR}/09_enterprise_ffmpeg_hwaccel_service.log" || true
./scripts/run_10_performance_diagnosis_playbook.sh | tee "${LOG_DIR}/10_performance_diagnosis_playbook.log" || true
./scripts/collect_env.sh "${LOG_DIR}/env" | tee "${LOG_DIR}/collect_env.log" || true
echo "[stage05 run_all] done"
