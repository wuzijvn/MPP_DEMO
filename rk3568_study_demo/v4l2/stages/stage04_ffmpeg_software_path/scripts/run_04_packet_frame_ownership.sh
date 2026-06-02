#!/usr/bin/env bash
set -euo pipefail
./bin/04_packet_frame_ownership --input="${INPUT:-./samples/sample.mp4}" --max-frames="${MAX_FRAMES:-6}"
