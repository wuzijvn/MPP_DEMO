#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/01_decoder_ioctl_sequence_map" \
  --codec="${CODEC:-H264}" \
  --raw="${RAW:-NV12}" \
  --frames="${FRAMES:-4}" \
  --source-change-at="${SOURCE_CHANGE_AT:-3}"
