#!/usr/bin/env bash
set -euo pipefail
./bin/03_two_queue_sequence_sim --out-dir="${OUT_DIR:-./logs/sim_sequence}" --output-tag="${OUTPUT_TAG:-demo03}" --dq-loops="${DQ_LOOPS:-3}"
