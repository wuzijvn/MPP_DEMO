#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/05_vm_vim2m_fault_injection" \
  --device="${DEVICE:-/dev/video0}" \
  --inject="${INJECT:-bytesused_zero}" \
  --verbose="${VERBOSE:-1}"
