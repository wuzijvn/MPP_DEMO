#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

LOG_DIR="${LOG_DIR:-${STAGE_DIR}/logs/12_enterprise_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${LOG_DIR}"

APP_PROJ_DIR="${STAGE_DIR}/enterprise_project"

# 把 stage03 入口与企业级子工程桥接起来，避免用户跳目录。
LOG_DIR="${LOG_DIR}" \
  "${APP_PROJ_DIR}/scripts/run_12_enterprise_m2m_pipeline_service.sh"
