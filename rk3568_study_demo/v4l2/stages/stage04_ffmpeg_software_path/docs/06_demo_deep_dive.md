# Stage04 Demo 深度解析

## 读代码顺序

1. `src/01_open_input_and_find_stream.cpp`
2. `src/02_demux_packet_loop.cpp`
3. `src/03_decode_packet_to_frame.cpp`
4. `src/04_packet_frame_ownership.cpp`
5. `src/05_pts_dts_timebase.cpp`
6. `src/06_save_yuv_frame.cpp`
7. `src/07_error_cleanup_pattern.cpp`
8. `src/08_cpu_decode_benchmark.cpp`

## Demo01 主流程解析

1. `avformat_open_input` 打开输入。
2. `avformat_find_stream_info` 读取流信息。
3. `av_find_best_stream` 定位视频流。
4. 可选 `av_dump_format` 打印容器细节。

关键结构体：`AVFormatContext`、`AVStream`。
关键函数：`avformat_open_input`、`avformat_find_stream_info`。
错误路径：任一步失败都释放 `fmt_ctx`。
驱动影子线：纯用户态容器解析，不触发硬解设备。

## Demo02 主流程解析

1. 循环 `av_read_frame` 取 packet。
2. 统计 video/audio packet 数。
3. 每轮 `av_packet_unref` 保持引用对称。

关键结构体：`AVPacket`。
关键函数：`av_read_frame`、`av_packet_unref`。
所有权：packet 从 demux 返回到 app，send 前后都需注意释放。

## Demo03 主流程解析

1. decoder 初始化：`avcodec_alloc_context3` / `avcodec_open2`。
2. 主循环：`send_packet -> receive_frame`。
3. 每帧输出分辨率/格式/pts，并 `av_frame_unref`。

关键结构体：`AVCodecContext`、`AVFrame`。
关键函数：`avcodec_send_packet`、`avcodec_receive_frame`。
错误路径：`goto cleanup` 统一释放 packet/frame/ctx。

## Demo04 数据流和所有权解析

1. `AVPacket`：demux 产生，send 后可 unref。
2. `AVFrame`：decoder 产出，应用处理后必须 unref。
3. 生命周期不对称会造成长跑内存上涨。

## Demo05 PTS/DTS/time_base 解析

1. `pts/dts` 必须结合 `AVStream.time_base` 解释。
2. `ts * av_q2d(tb)` 得到秒级时间。
3. `AV_NOPTS_VALUE` 要单独处理。

## Demo06 保存 YUV420P 解析

1. 仅保存 `AV_PIX_FMT_YUV420P`。
2. 写文件时按 `linesize` 行拷贝，不能假设紧密排布。
3. 非 YUV420P 帧在本 demo 跳过。

## Demo07 错误路径与资源释放解析

1. 用 `--inject-step` 人工触发失败。
2. 所有失败统一 `goto fail`。
3. `fail` 中对 `frame/pkt/dec_ctx/fmt_ctx` 做对称释放。

## Demo08 性能观察解析

1. 用 `av_gettime_relative` 统计耗时。
2. `fps = frame_count / elapsed_s`。
3. 结果是“软件基线”，用于和 Stage05 对比。

## 常见问题（问题-原因-解决方向）

1. 现象：`avformat_open_input` 失败。
最可能原因：输入路径错误或容器不支持。
属于哪一层：用户态命令层。
验证命令：`ffprobe input.mp4`。
解决方向：更换样本，检查权限。
如果仍失败：看 FFmpeg 构建能力。

2. 现象：`avcodec_open2` 失败。
最可能原因：decoder 不可用或参数异常。
属于哪一层：框架层。
验证命令：`ffmpeg -decoders | grep h264`。
解决方向：确认 codec id 和 decoder 可用性。
如果仍失败：检查 FFmpeg 版本与编译配置。

3. 现象：frame 数极少或 0。
最可能原因：输入流不是目标视频流或 send/receive 顺序错误。
属于哪一层：框架层。
验证命令：增加 verbose 日志。
解决方向：检查 `stream_index` 过滤与循环条件。
如果仍失败：输出 packet pts/dts 做进一步定位。

4. 现象：保存 YUV 失败。
最可能原因：帧格式不是 YUV420P 或文件路径不可写。
属于哪一层：应用层。
验证命令：看 `frame->format` 与 `ls -ld output_dir`。
解决方向：先做 swscale 转换或换样本。
如果仍失败：检查文件系统权限和剩余空间。

5. 现象：长跑内存上涨。
最可能原因：漏掉 `av_packet_unref` 或 `av_frame_unref`。
属于哪一层：应用资源管理层。
验证命令：循环跑 + `top`/`pmap` 观察。
解决方向：逐分支补齐 unref/free 对称。
如果仍失败：用 valgrind/asan 做更细粒度检查。

## 驱动影子线：这一阶段对应的驱动侧知识

1. Stage04 纯软件解码不依赖 `/dev/videoX` 或 `/dev/dri/renderD*`。
2. 它是 Stage05 硬件路径排障的对照组。
3. 若同一输入在 Stage04 通过而 Stage05 失败，优先查 hwaccel backend/设备节点/驱动。

## 面试表达模板

“我先用 Stage04 建立软件解码基线，明确 packet/frame 所有权、PTS/DTS 解释和 cleanup 对称释放。后续硬件路径异常时，我会先回归这个基线，隔离输入问题和框架/驱动问题。”
