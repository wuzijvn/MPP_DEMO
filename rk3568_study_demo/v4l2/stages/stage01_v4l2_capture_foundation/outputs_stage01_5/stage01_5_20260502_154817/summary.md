# Stage01.5 Summary

- run_tag: 20260502_154817 (UTC)
- device: /dev/video10
- frames(default): 120
- warmup: 3
- req_fps: 30
- timeout_ms: 2000
- log_every: 20

## Table

| case | fps | timeout | dq_fail | dq_eagain | rq_fail | rq_skipped | bytes(min/max/avg) | dq_avg_ms | v4l2_avg_ms | ret |
|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|
| A_baseline_640x480_yuyv_buf4 | 4.936 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.423 | 202.426 | 0 |
| B_reqbufs_2_640x480_yuyv | 4.939 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.423 | 202.426 | 0 |
| B_reqbufs_4_640x480_yuyv | 4.939 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.425 | 202.427 | 0 |
| B_reqbufs_8_640x480_yuyv | 4.881 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.423 | 202.426 | 0 |
| C_pixfmt_YUYV_640x480 | 4.866 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.421 | 202.425 | 0 |
| C_pixfmt_MJPG_640x480 | 4.936 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.398 | 202.393 | 0 |
| D_res_640x480_yuyv | 4.919 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.423 | 202.426 | 0 |
| D_res_1280x720_yuyv | 4.915 | 0 | 0 | 0 | 0 | 0 | 614400/614400/614400.00 | 202.423 | 202.426 | 0 |
| E_inject_skip_requeue_f20 | 4.584 | 1 | 0 | 0 | 0 | 4 | 614400/614400/614400.00 | 202.376 | 202.370 | 2 |

## Raw

- logs: /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/outputs_stage01_5/stage01_5_20260502_154817/logs
- summary.csv: /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/outputs_stage01_5/stage01_5_20260502_154817/summary.csv
