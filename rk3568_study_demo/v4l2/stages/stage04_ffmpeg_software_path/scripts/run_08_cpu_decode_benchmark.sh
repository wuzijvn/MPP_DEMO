#!/usr/bin/env bash
set -euo pipefail
./bin/08_cpu_decode_benchmark --input="${INPUT:-./samples/sample.mp4}" --max-frames="${MAX_FRAMES:-120}"
