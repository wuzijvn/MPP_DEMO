#!/usr/bin/env bash
set -euo pipefail
OUT="${OUT:-./logs/demo06_first_frame.yuv}"
mkdir -p "$(dirname "${OUT}")"
./bin/06_save_yuv_frame --input="${INPUT:-./samples/sample.mp4}" --output="${OUT}"
