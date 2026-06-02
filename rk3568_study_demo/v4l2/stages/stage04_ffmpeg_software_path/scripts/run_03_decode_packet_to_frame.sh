#!/usr/bin/env bash
set -euo pipefail
./bin/03_decode_packet_to_frame --input="${INPUT:-./samples/sample.mp4}" --max-frames="${MAX_FRAMES:-10}" --verbose
