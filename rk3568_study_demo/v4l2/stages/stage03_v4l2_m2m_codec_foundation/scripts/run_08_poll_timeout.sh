#!/usr/bin/env bash
set -euo pipefail

APP="./bin/08_poll_timeout"
DEV="${DEV:-/dev/video0}"
TIMEOUT_MS="${TIMEOUT_MS:-80}"

"${APP}" --dev="${DEV}" --timeout-ms="${TIMEOUT_MS}" --verbose
