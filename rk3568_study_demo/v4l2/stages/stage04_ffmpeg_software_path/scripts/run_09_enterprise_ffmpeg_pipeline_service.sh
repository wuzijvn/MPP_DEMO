#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOG_DIR="${LOG_DIR:-${STAGE_DIR}/logs/09_enterprise_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${LOG_DIR}"
LOG_DIR="${LOG_DIR}" "${STAGE_DIR}/enterprise_project/scripts/run_09_enterprise_ffmpeg_pipeline_service.sh"
