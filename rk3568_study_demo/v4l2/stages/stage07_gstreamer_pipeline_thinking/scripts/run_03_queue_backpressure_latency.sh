#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/03_queue_backpressure_latency" \
  --frames="${FRAMES:-40}" \
  --sleep-us="${SLEEP_US:-2000}" \
  --queue-depth="${QUEUE_DEPTH:-4}"
