#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT="${OUT:-${STAGE_DIR}/logs/06_timeout_debug_report.md}"
"${STAGE_DIR}/bin/06_timeout_debug_report_template" \
  --scenario="${SCENARIO:-dqbuf_timeout}" \
  --timeouts="${TIMEOUTS:-1}" \
  --bytesused-zero="${BYTESUSED_ZERO:-0}" \
  --source-change="${SOURCE_CHANGE:-0}" \
  --out="${OUT}"
