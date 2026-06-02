#!/usr/bin/env bash
set -euo pipefail

LOG_DIR="${1:-./logs/run_all_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${LOG_DIR}"

echo "[stage03 run_all] log_dir=${LOG_DIR}"

./build.sh all | tee "${LOG_DIR}/build.log"

./scripts/run_01_open_video_device.sh | tee "${LOG_DIR}/01_open_video_device.log" || true
./scripts/run_02_querycap_enum_formats.sh | tee "${LOG_DIR}/02_querycap_enum_formats.log" || true
./scripts/run_03_two_queue_sequence_sim.sh | tee "${LOG_DIR}/03_two_queue_sequence_sim.log" || true
./scripts/run_04_try_set_format.sh | tee "${LOG_DIR}/04_try_set_format.log" || true
./scripts/run_05_request_query_mmap.sh | tee "${LOG_DIR}/05_request_query_mmap.log" || true
./scripts/run_06_qbuf_dqbuf_ownership.sh | tee "${LOG_DIR}/06_qbuf_dqbuf_ownership.log" || true
./scripts/run_07_streamon_streamoff.sh | tee "${LOG_DIR}/07_streamon_streamoff.log" || true
./scripts/run_08_poll_timeout.sh | tee "${LOG_DIR}/08_poll_timeout.log" || true
./scripts/run_09_m2m_decoder_sequence_pseudocode.sh | tee "${LOG_DIR}/09_m2m_decoder_sequence_pseudocode.log" || true
./scripts/run_10_source_change_eos_drain_note.sh | tee "${LOG_DIR}/10_source_change_eos_drain_note.log" || true
./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh | tee "${LOG_DIR}/11_bitstream_payload_to_qbuf_bytesused.log" || true
./scripts/run_12_enterprise_m2m_pipeline_service.sh | tee "${LOG_DIR}/12_enterprise_m2m_pipeline_service.log" || true

./scripts/collect_env.sh "${LOG_DIR}/env" | tee "${LOG_DIR}/collect_env.log" || true

echo "[stage03 run_all] done"
