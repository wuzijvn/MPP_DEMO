#!/usr/bin/env bash
set -euo pipefail

APP="./bin/11_bitstream_payload_to_qbuf_bytesused"
INPUT="${INPUT:-}"
CODEC="${CODEC:-h264}"
MAX_OUTPUT_BYTES="${MAX_OUTPUT_BYTES:-0}"

args=(
  "--codec=${CODEC}"
  "--max-output-bytes=${MAX_OUTPUT_BYTES}"
  "--verbose"
)

if [[ -n "${INPUT}" ]]; then
  args+=("--input=${INPUT}")
fi

"${APP}" "${args[@]}"
