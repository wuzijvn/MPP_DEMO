# Stage04 代码阅读顺序

1. `include/00_ffmpeg_demo_common.hpp`
2. `src/01_open_input_and_find_stream.cpp`
3. `src/02_demux_packet_loop.cpp`
4. `src/03_decode_packet_to_frame.cpp`
5. `src/04_packet_frame_ownership.cpp`
6. `src/07_error_cleanup_pattern.cpp`
7. `src/08_cpu_decode_benchmark.cpp`

重点看：
1. 每次 `av_read_frame` 后是否 `av_packet_unref`。
2. `avcodec_receive_frame` 的 `EAGAIN/EOF` 分支。
3. `goto fail` 的资源对称释放。
