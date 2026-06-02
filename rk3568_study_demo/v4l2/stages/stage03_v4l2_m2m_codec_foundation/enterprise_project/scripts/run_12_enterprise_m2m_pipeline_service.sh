#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

LOG_DIR="${LOG_DIR:-${PROJ_DIR}/logs/run_$(date +%Y%m%d_%H%M%S)}"
DEV="${DEV:-/dev/video0}"
IN_FOURCC="${IN_FOURCC:-H264}"
OUT_FOURCC="${OUT_FOURCC:-NV12}"
WIDTH="${WIDTH:-1280}"
HEIGHT="${HEIGHT:-720}"
OUT_COUNT="${OUT_COUNT:-4}"
CAP_COUNT="${CAP_COUNT:-4}"
TIMEOUT_MS="${TIMEOUT_MS:-200}"
LOOPS="${LOOPS:-8}"
MPLANE="${MPLANE:-0}"
OUTPUT_BYTESUSED="${OUTPUT_BYTESUSED:-16}"
INPUT_ANNEXB="${INPUT_ANNEXB:-}"
MAX_INPUT_CHUNKS="${MAX_INPUT_CHUNKS:-0}"
INJECT_TIMEOUT="${INJECT_TIMEOUT:-0}"
INJECT_SOURCE_CHANGE="${INJECT_SOURCE_CHANGE:-0}"
INJECT_DQBUF_EAGAIN="${INJECT_DQBUF_EAGAIN:-0}"
VERBOSE="${VERBOSE:-1}"

mkdir -p "${LOG_DIR}"

"${PROJ_DIR}/build.sh"

APP="${PROJ_DIR}/bin/12_enterprise_m2m_pipeline_service"

args=(
  "--dev=${DEV}"
  "--in-fourcc=${IN_FOURCC}"
  "--out-fourcc=${OUT_FOURCC}"
  "--width=${WIDTH}"
  "--height=${HEIGHT}"
  "--out-count=${OUT_COUNT}"
  "--cap-count=${CAP_COUNT}"
  "--timeout-ms=${TIMEOUT_MS}"
  "--loops=${LOOPS}"
  "--mplane=${MPLANE}"
  "--output-bytesused=${OUTPUT_BYTESUSED}"
  "--max-input-chunks=${MAX_INPUT_CHUNKS}"
  "--inject-timeout=${INJECT_TIMEOUT}"
  "--inject-source-change=${INJECT_SOURCE_CHANGE}"
  "--inject-dqbuf-eagain=${INJECT_DQBUF_EAGAIN}"
  "--log-dir=${LOG_DIR}"
)

if [[ -n "${INPUT_ANNEXB}" ]]; then
  args+=("--input-annexb=${INPUT_ANNEXB}")
fi

if [[ "${VERBOSE}" == "1" ]]; then
  args+=("--verbose")
fi

echo "[run12] app=${APP}"
echo "[run12] log_dir=${LOG_DIR}"
"${APP}" "${args[@]}"
