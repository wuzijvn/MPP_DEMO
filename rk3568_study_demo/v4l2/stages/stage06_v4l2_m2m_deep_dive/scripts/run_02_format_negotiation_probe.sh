#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/02_format_negotiation_probe" \
  --device="${DEVICE:-/dev/video0}" \
  --output="${OUTPUT:-H264}" \
  --capture="${CAPTURE:-NV12}" \
  --width="${WIDTH:-1280}" \
  --height="${HEIGHT:-720}" \
  --mplane="${MPLANE:-1}" \
  --apply="${APPLY:-0}" \
  --simulate="${SIMULATE:-0}" \
  --require-device="${REQUIRE_DEVICE:-0}"
