# Stage04 主执行指南

## A. 构建

```bash
./build.sh all
```

## B. 单 demo

```bash
./scripts/run_01_open_input_and_find_stream.sh INPUT=./samples/sample.mp4
./scripts/run_02_demux_packet_loop.sh INPUT=./samples/sample.mp4 MAX_PACKETS=20
./scripts/run_03_decode_packet_to_frame.sh INPUT=./samples/sample.mp4 MAX_FRAMES=10
./scripts/run_04_packet_frame_ownership.sh INPUT=./samples/sample.mp4 MAX_FRAMES=6
./scripts/run_05_pts_dts_timebase.sh INPUT=./samples/sample.mp4 MAX_PACKETS=20
./scripts/run_06_save_yuv_frame.sh INPUT=./samples/sample.mp4 OUT=./logs/demo06_first_frame.yuv
./scripts/run_07_error_cleanup_pattern.sh INPUT=./samples/sample.mp4 INJECT_STEP=2
./scripts/run_08_cpu_decode_benchmark.sh INPUT=./samples/sample.mp4 MAX_FRAMES=120
```

## C. 企业级项目

```bash
./scripts/run_09_enterprise_ffmpeg_pipeline_service.sh
```

## D. 一键执行

```bash
./scripts/run_all_stage04.sh
```
