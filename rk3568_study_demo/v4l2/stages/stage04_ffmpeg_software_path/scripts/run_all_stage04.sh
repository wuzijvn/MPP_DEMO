#!/usr/bin/env bash
set -euo pipefail
LOG_DIR="${1:-./logs/run_all_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${LOG_DIR}"
echo "[stage04 run_all] log_dir=${LOG_DIR}"
./build.sh all | tee "${LOG_DIR}/build.log" || true
./scripts/run_01_open_input_and_find_stream.sh | tee "${LOG_DIR}/01_open_input_and_find_stream.log" || true
./scripts/run_02_demux_packet_loop.sh | tee "${LOG_DIR}/02_demux_packet_loop.log" || true
./scripts/run_03_decode_packet_to_frame.sh | tee "${LOG_DIR}/03_decode_packet_to_frame.log" || true
./scripts/run_04_packet_frame_ownership.sh | tee "${LOG_DIR}/04_packet_frame_ownership.log" || true
./scripts/run_05_pts_dts_timebase.sh | tee "${LOG_DIR}/05_pts_dts_timebase.log" || true
./scripts/run_06_save_yuv_frame.sh | tee "${LOG_DIR}/06_save_yuv_frame.log" || true
./scripts/run_07_error_cleanup_pattern.sh | tee "${LOG_DIR}/07_error_cleanup_pattern.log" || true
./scripts/run_08_cpu_decode_benchmark.sh | tee "${LOG_DIR}/08_cpu_decode_benchmark.log" || true
./scripts/run_09_enterprise_ffmpeg_pipeline_service.sh | tee "${LOG_DIR}/09_enterprise_ffmpeg_pipeline_service.log" || true
./scripts/collect_env.sh "${LOG_DIR}/env" | tee "${LOG_DIR}/collect_env.log" || true
echo "[stage04 run_all] done"
