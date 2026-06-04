#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
args=()
if [[ "${REQUIRE_HW:-0}" == "1" ]]; then
  args+=(--require-hw)
fi
"${STAGE_DIR}/bin/07_hardware_backend_probe" "${args[@]}"
