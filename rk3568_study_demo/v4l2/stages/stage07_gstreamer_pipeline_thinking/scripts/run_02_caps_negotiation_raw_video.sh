#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/02_caps_negotiation_raw_video" \
  --width="${WIDTH:-320}" \
  --height="${HEIGHT:-240}" \
  --frames="${FRAMES:-8}" \
  --in-format="${IN_FORMAT:-NV12}" \
  --out-format="${OUT_FORMAT:-I420}"
