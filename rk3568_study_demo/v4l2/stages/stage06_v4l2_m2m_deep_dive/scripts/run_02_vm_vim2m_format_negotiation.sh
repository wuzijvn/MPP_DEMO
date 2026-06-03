#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/02_vm_vim2m_format_negotiation" \
  --device="${DEVICE:-/dev/video0}" \
  --output="${OUTPUT:-RGBP}" \
  --capture="${CAPTURE:-RGBP}" \
  --width="${WIDTH:-640}" \
  --height="${HEIGHT:-480}" \
  --apply="${APPLY:-0}" \
  --verbose="${VERBOSE:-1}"
