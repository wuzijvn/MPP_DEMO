# Stage01 Matrix Report

- run_tag: 20260501_061457 (UTC)
- device: /dev/video999
- frames: 10
- warmup: 3
- req_fps: 30
- timeout_ms: 2000
- req_bufs: 4
- log_every: 100

## Summary Table

| case | req | pixfmt | ret | active_fmt | fps | timeout | dq_fail | requeue_fail | skipped | trace_csv |
|---|---|---|---:|---|---:|---:|---:|---:|---:|---|
| 640x480_YUYV | 640x480 | YUYV | 1 | NA | NA | NA | NA | NA | NA | ./outputs_test_baddev/stage01_matrix_20260501_061457/traces/640x480_YUYV.csv |
| 640x480_NV12 | 640x480 | NV12 | 1 | NA | NA | NA | NA | NA | NA | ./outputs_test_baddev/stage01_matrix_20260501_061457/traces/640x480_NV12.csv |
| 640x480_MJPG | 640x480 | MJPG | 1 | NA | NA | NA | NA | NA | NA | ./outputs_test_baddev/stage01_matrix_20260501_061457/traces/640x480_MJPG.csv |
| 1280x720_YUYV | 1280x720 | YUYV | 1 | NA | NA | NA | NA | NA | NA | ./outputs_test_baddev/stage01_matrix_20260501_061457/traces/1280x720_YUYV.csv |
| 1280x720_NV12 | 1280x720 | NV12 | 1 | NA | NA | NA | NA | NA | NA | ./outputs_test_baddev/stage01_matrix_20260501_061457/traces/1280x720_NV12.csv |
| 1280x720_MJPG | 1280x720 | MJPG | 1 | NA | NA | NA | NA | NA | NA | ./outputs_test_baddev/stage01_matrix_20260501_061457/traces/1280x720_MJPG.csv |

## Notes

- ret=0：达到目标帧数；ret=2：采集中途提前结束（常见于超时/注入）。
- 请优先关注：
  1) active format snapshot 是否与请求一致
  2) fps 与 timeout/dq_fail 的关联
  3) trace CSV 中 host_delta_ms/v4l2_delta_ms 是否稳定
  4) buffer flags 分布是否出现 ERROR

## Raw Artifacts

- logs: ./outputs_test_baddev/stage01_matrix_20260501_061457/logs
- traces: ./outputs_test_baddev/stage01_matrix_20260501_061457/traces
- preview: ./outputs_test_baddev/stage01_matrix_20260501_061457/preview
