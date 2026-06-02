#!/usr/bin/env bash
set -euo pipefail
./bin/02_demux_packet_loop --input="${INPUT:-./samples/sample.mp4}" --max-packets="${MAX_PACKETS:-30}" --verbose
