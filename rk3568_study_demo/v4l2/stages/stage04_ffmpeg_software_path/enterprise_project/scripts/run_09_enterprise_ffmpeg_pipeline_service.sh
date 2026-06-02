#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOG_DIR="${LOG_DIR:-${PROJ_DIR}/logs/run_$(date +%Y%m%d_%H%M%S)}"
INPUT="${INPUT:-${PROJ_DIR}/../samples/sample.mp4}"
MAX_FRAMES="${MAX_FRAMES:-120}"
INJECT_SEND_FAIL="${INJECT_SEND_FAIL:-0}"
INJECT_RECEIVE_FAIL="${INJECT_RECEIVE_FAIL:-0}"
VERBOSE="${VERBOSE:-1}"
mkdir -p "${LOG_DIR}"
"${PROJ_DIR}/build.sh"
ARGS=("--input=${INPUT}" "--log-dir=${LOG_DIR}" "--max-frames=${MAX_FRAMES}")
if [[ "${INJECT_SEND_FAIL}" == "1" ]]; then ARGS+=("--inject-send-fail"); fi
if [[ "${INJECT_RECEIVE_FAIL}" == "1" ]]; then ARGS+=("--inject-receive-fail"); fi
if [[ "${VERBOSE}" == "1" ]]; then ARGS+=("--verbose"); fi
"${PROJ_DIR}/bin/09_enterprise_ffmpeg_pipeline_service" "${ARGS[@]}"
