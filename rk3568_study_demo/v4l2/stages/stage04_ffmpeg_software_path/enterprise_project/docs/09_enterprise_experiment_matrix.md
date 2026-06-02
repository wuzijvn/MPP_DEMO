# 09 Enterprise Experiment Matrix

| 实验 | 命令 | 指标 | 通过标准 |
| --- | --- | --- | --- |
| normal | `run_09_enterprise_ffmpeg_pipeline_service.sh` | frame_out/fps | result=PASS |
| inject_send_fail | `INJECT_SEND_FAIL=1 ...` | error_count | error_count 上升且流程可收敛 |
| inject_receive_fail | `INJECT_RECEIVE_FAIL=1 ...` | error_count | error_count 上升且日志可解释 |
