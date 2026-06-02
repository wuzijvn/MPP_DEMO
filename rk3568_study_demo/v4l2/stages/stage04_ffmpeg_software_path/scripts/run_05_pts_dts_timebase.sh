#!/usr/bin/env bash
set -euo pipefail
./bin/05_pts_dts_timebase --input="${INPUT:-./samples/sample.mp4}" --max-packets="${MAX_PACKETS:-20}"
