#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/01_vm_vim2m_device_discovery" \
  --device="${DEVICE:-/dev/video0}" \
  --require-m2m="${REQUIRE_M2M:-1}" \
  --verbose="${VERBOSE:-1}"
