#!/usr/bin/env bash
set -euo pipefail
./bin/07_error_cleanup_pattern --input="${INPUT:-./samples/sample.mp4}" --inject-step="${INJECT_STEP:-0}"
