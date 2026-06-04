#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
args=(--frames="${FRAMES:-12}")
if [[ "${SKIP_FFMPEG:-0}" == "1" ]]; then
  args+=(--skip-ffmpeg)
fi
"${STAGE_DIR}/bin/06_ffmpeg_gstreamer_compare" "${args[@]}"
