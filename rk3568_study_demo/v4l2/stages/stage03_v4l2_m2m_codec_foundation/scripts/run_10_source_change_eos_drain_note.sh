#!/usr/bin/env bash
set -euo pipefail

APP="./bin/10_source_change_eos_drain_note"
DEV="${DEV:-/dev/video0}"
TIMEOUT_MS="${TIMEOUT_MS:-120}"

"${APP}" --dev="${DEV}" --timeout-ms="${TIMEOUT_MS}" --verbose
