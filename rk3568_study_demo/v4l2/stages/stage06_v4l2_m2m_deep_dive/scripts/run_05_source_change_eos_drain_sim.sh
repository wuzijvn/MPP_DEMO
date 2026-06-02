#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/05_source_change_eos_drain_sim" \
  --frames="${FRAMES:-10}" \
  --source-change-at="${SOURCE_CHANGE_AT:-4}" \
  --eos-at="${EOS_AT:-10}" \
  --bad-reconfigure="${BAD_RECONFIGURE:-0}"
