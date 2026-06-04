# Stage06 Post-Code Debugging Guide

这份文档不是脚本。它是一份上线后问题的手动调试指导书：当 Stage06 代码已经能跑，但你开始怀疑 CPU 高、耗时异常、内存泄漏、fd 泄漏、queue 卡住、硬解 fallback 或 RK 板端硬件路径不稳时，跟着这里一步一步收证据。

Stage06 有两条线：

| 线 | 用来验证什么 | 不要误判 |
| --- | --- | --- |
| VM/vim2m | 真实 V4L2 M2M ioctl、mmap、QBUF/DQBUF/poll 逻辑 | 不证明 H.264/H.265 硬解 |
| RK/RKMPP | RK 板端 FFmpeg/RKMPP 或厂商工具硬件路径证据 | 不把 ISP/camera 节点当 codec M2M |

## 0. 先建立基线

进入 stage06：

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive
```

重新编译，并保留调试符号：

```bash
CXXFLAGS="-std=c++11 -g -O0 -Wall -Wextra" ./build.sh all-with-enterprise
```

先跑一次正常路径：

```bash
./scripts/run_04_vm_vim2m_queue_loop.sh
./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

你要先确认正常日志里有这些证据：

```text
QBUF VIDEO_OUTPUT
QBUF VIDEO_CAPTURE
STREAMON
poll ret=1
DQBUF VIDEO_CAPTURE
DQBUF VIDEO_OUTPUT
STREAMOFF
```

如果这些都没有，先不要看性能。因为这说明你还没有进入 Stage06 的核心 queue loop。

## 1. 症状速查表

| 症状 | 先看什么 | 常见原因 |
| --- | --- | --- |
| CPU 高 | `top -H`、`pidstat`、`perf top` | 忙等、重复 memcpy、poll timeout 后循环过快 |
| 耗时高 | `/usr/bin/time -v`、`strace -T` | `poll` 等待过长、DQBUF 不返回、软件 fallback |
| 内存涨 | `ps`、`pmap`、`smaps_rollup`、Valgrind/ASan | buffer 未释放、mmap 未 munmap、容器持续增长 |
| fd 涨 | `/proc/<pid>/fd`、`lsof -p`、`strace openat/close` | open 后异常路径没 close |
| queue 卡住 | metrics、`strace ioctl,poll` | OUTPUT/CAPTURE 未同时排队、bytesused 错、STREAMON 顺序错 |
| RK 硬解不确定 | FFmpeg decoder、dmesg、设备节点 | decoder 不存在、软件解码 fallback、输入码流不匹配 |

## 2. CPU 占用调试

先让程序跑久一点，方便观察。企业服务适合做这个练习：

```bash
FRAMES=10000 VERBOSE=0 ./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh &
PID=$!
```

看进程整体 CPU：

```bash
top -p "$PID"
```

看线程级 CPU：

```bash
top -H -p "$PID"
```

如果系统有 `pidstat`：

```bash
pidstat -p "$PID" 1
```

正常现象：

```text
CPU 有波动，但不会长时间无意义地打满。
poll 等待期间 CPU 应下降。
```

异常现象：

```text
poll timeout 后立即重试，没有退避。
DQBUF 失败后 while 循环继续空转。
CPU 主要消耗在 memcpy/memset，说明可能有额外 copy。
```

如果系统有 `perf`，继续看热点：

```bash
perf top -p "$PID"
```

或抓 10 秒采样：

```bash
perf record -g -p "$PID" -- sleep 10
perf report
```

解释方法：

| 热点 | 推断 |
| --- | --- |
| `memcpy` / `memset` 很高 | copy 路径过多，后续要查格式转换、hwdownload、buffer copy |
| `ioctl` / `poll` 很高 | 系统调用频繁，可能 queue loop 太紧 |
| 自己的 while/for 函数很高 | 用户态忙等或错误恢复路径没有节流 |

结束后台进程：

```bash
kill "$PID" 2>/dev/null || true
```

## 3. 耗时和 latency 调试

先看整体耗时和最大 RSS：

```bash
/usr/bin/time -v ./scripts/run_04_vm_vim2m_queue_loop.sh
```

重点看：

```text
Elapsed (wall clock) time
User time
System time
Maximum resident set size
Voluntary context switches
Involuntary context switches
```

如果整体耗时高，用 `strace -T` 看每个 syscall 的耗时：

```bash
strace -tt -T -e ioctl,poll,mmap,munmap,openat,close \
  ./scripts/run_04_vm_vim2m_queue_loop.sh
```

正常现象：

```text
poll 有等待时间，但最终返回可读/可写事件。
VIDIOC_DQBUF 能持续返回 OUTPUT/CAPTURE buffer。
```

异常现象：

```text
poll(... ) = 0 表示 timeout。
VIDIOC_DQBUF 反复 EAGAIN，说明没有 buffer 完成。
VIDIOC_QBUF 失败，说明 queue 参数、buffer index、bytesused 或格式可能错了。
```

企业服务会写 metrics。跑完后看：

```bash
sed -n '1,220p' enterprise_project/logs/run_default/enterprise_metrics.json
```

重点字段：

```text
poll_calls
timeout_count
qbuf_output
qbuf_capture
dqbuf_output
dqbuf_capture
decoded_frames
failure_layer
```

解释方法：

| 指标 | 正常 | 异常 |
| --- | --- | --- |
| `timeout_count` | 0 或可解释 | 持续增加说明 driver 没有完成 buffer |
| `qbuf_*` | 大于 0 | 为 0 说明 buffer 没交给 driver |
| `dqbuf_*` | 大于 0 | 为 0 说明 driver 没把 buffer 交回来 |
| `decoded_frames` | 达到 gate | 低于 gate 要看 timeout、bytesused、format |

## 4. 内存泄漏和内存增长调试

先观察运行中 RSS 是否持续增长：

```bash
FRAMES=10000 VERBOSE=0 ./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh &
PID=$!
watch -n 1 "ps -p $PID -o pid,etime,rss,vsz,cmd"
```

RSS 如果只在启动阶段上涨，然后稳定，通常是一次性分配。RSS 如果每处理一批帧都上涨，就要怀疑泄漏。

看进程内存映射：

```bash
pmap -x "$PID" | tail -n 20
```

如果系统支持：

```bash
cat "/proc/$PID/smaps_rollup"
```

结束后台进程：

```bash
kill "$PID" 2>/dev/null || true
```

用 Valgrind 查泄漏和 fd：

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
  ./bin/04_vm_vim2m_queue_loop
```

正常现象：

```text
definitely lost: 0 bytes
open file descriptors at exit 不包含未关闭的 /dev/videoX
```

异常现象：

```text
definitely lost 增长：堆内存泄漏。
still reachable 很大：可能是全局缓存，也可能是清理不完整，需要结合代码判断。
open fd at exit 指向 /dev/video0：异常路径没 close。
```

用 ASan/LSan 重新编译：

```bash
CXXFLAGS="-std=c++11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra" \
LDFLAGS="-fsanitize=address,undefined" \
  ./build.sh all-with-enterprise
```

运行：

```bash
ASAN_OPTIONS=detect_leaks=1 ./scripts/run_04_vm_vim2m_queue_loop.sh
```

ASan 适合抓：

```text
heap-use-after-free
double-free
buffer overflow
leak
undefined behavior
```

调试完再恢复普通构建：

```bash
CXXFLAGS="-std=c++11 -g -O0 -Wall -Wextra" ./build.sh all-with-enterprise
```

## 5. fd 和 mmap 生命周期调试

fd 泄漏常见于错误路径：`open` 成功后，中间 `ioctl` 失败，函数直接返回，忘了 `close`。

运行长一点后看 fd 数量：

```bash
FRAMES=10000 VERBOSE=0 ./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh &
PID=$!
ls -l "/proc/$PID/fd"
ls -l "/proc/$PID/fd" | wc -l
```

如果系统有 `lsof`：

```bash
lsof -p "$PID"
```

结束：

```bash
kill "$PID" 2>/dev/null || true
```

用 strace 对称检查：

```bash
strace -ff -o logs/manual_strace_vm_queue \
  -e openat,close,mmap,munmap,ioctl,poll \
  ./scripts/run_04_vm_vim2m_queue_loop.sh
```

看 trace：

```bash
grep -R "openat\\|close\\|mmap\\|munmap\\|VIDIOC_STREAMOFF\\|VIDIOC_REQBUFS" logs/manual_strace_vm_queue*
```

Stage06 VM queue loop 里，你要重点确认：

```text
/dev/videoX open 后最终 close。
每个 mmap 成功的 buffer 最终 munmap。
STREAMON 后最终 STREAMOFF。
必要时 REQBUFS count=0 释放 driver queue buffer。
```

异常解释：

| 现象 | 可能原因 |
| --- | --- |
| `openat("/dev/video0")` 多于 `close` | fd 泄漏 |
| `mmap` 多于 `munmap` | buffer 映射泄漏 |
| 有 `STREAMON` 无 `STREAMOFF` | 异常退出清理不完整 |
| `REQBUFS count=0` 不出现 | driver 侧 buffer 释放可能不完整 |

## 6. V4L2 M2M queue 健康检查

Stage06 的核心不是“能打开设备”，而是 buffer ownership 是否真的在 user 和 driver 之间来回移动。

正常路径至少要看到：

```text
QBUF VIDEO_OUTPUT
QBUF VIDEO_CAPTURE
STREAMON OUTPUT
STREAMON CAPTURE
poll ret=1
DQBUF VIDEO_CAPTURE
DQBUF VIDEO_OUTPUT
requeue CAPTURE
STREAMOFF
```

企业 metrics 里至少要看到：

```text
real_ioctl_path=true
mapped_output > 0
mapped_capture > 0
qbuf_output > 0
qbuf_capture > 0
dqbuf_output > 0
dqbuf_capture > 0
```

常见故障判断：

| 故障 | 你会看到 | 下一步 |
| --- | --- | --- |
| OUTPUT `bytesused=0` | QBUF 被拒绝或 gate fail | 检查压缩/原始输入 buffer 是否写入有效数据 |
| CAPTURE 没排队 | poll timeout 或 DQBUF 为空 | 先排 CAPTURE buffer，再启动 streaming |
| 只 QBUF 不 DQBUF | queue 没完成 | 查格式、STREAMON 顺序、driver log |
| timeout 可恢复 | recovery 后继续有 DQBUF | 确认恢复次数和耗时可接受 |
| source change 后 timeout | CAPTURE queue 未重配 | 需要 STREAMOFF/REQBUFS/S_FMT/REQBUFS/STREAMON |

## 7. copy path 和 software fallback 调试

在 VM `vim2m` 上，它是 raw-to-raw 虚拟 M2M，重点是 queue 逻辑，不是硬解性能。

如果是在 FFmpeg/RK 路径上，要确认有没有软件 fallback：

```bash
ffmpeg -hide_banner -decoders | grep -Ei 'rkmpp|v4l2|h264|hevc'
```

有码流时：

```bash
ffmpeg -hide_banner -benchmark -v verbose \
  -c:v h264_rkmpp -i /path/to/sample.h264 -f null -
```

重点看：

```text
是否真的选择 h264_rkmpp/hevc_rkmpp。
日志里是否出现软件 decoder。
是否出现 hwdownload、format conversion、额外 swscale。
benchmark 的 user/system/real time 是否异常。
```

如果 `perf` 里看到大量 `memcpy/memset`，结合 FFmpeg/GStreamer 日志判断：

| 现象 | 推断 |
| --- | --- |
| `memcpy` 很高 | 可能发生额外拷贝、格式转换、dump frame |
| `swscale` 出现 | 可能走了软件颜色空间转换 |
| `hwdownload` 出现 | 硬件帧被下载回 CPU，零拷贝断了 |
| decoder 名称不是 RKMPP/V4L2 | 可能 fallback 到软件解码 |

## 8. RK 板硬件路径调试

RK 板上不要强行把 camera/ISP 节点当 codec M2M。先跑 Stage06 的 RK 证据路径：

```bash
./scripts/run_06_rk_board_rkmpp_hardware_path.sh
```

有码流时：

```bash
INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./scripts/run_06_rk_board_rkmpp_hardware_path.sh
```

企业服务 RK 模式：

```bash
MODE=rk-rkmpp INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

看 FFmpeg/RKMPP 能力：

```bash
ffmpeg -hide_banner -decoders | grep -Ei 'rkmpp|h264|hevc'
```

看板端日志：

```bash
dmesg | grep -Ei 'rkvdec|mpp|vpu|codec|firmware|timeout|reset|dma|iommu|thermal|clk|pm'
```

看温度，路径存在时再执行：

```bash
for z in /sys/class/thermal/thermal_zone*/temp; do echo "$z=$(cat "$z")"; done
```

可能的硬件侧解释：

| 证据 | 可能原因 |
| --- | --- |
| 无 RKMPP decoder | FFmpeg 未编 RKMPP，或板端包不支持 |
| dmesg 有 firmware/reset/timeout | VPU firmware、码流、驱动恢复问题 |
| thermal 温度高且性能下降 | 热降频可能影响吞吐 |
| 日志显示 software decoder | 没进入硬解路径 |
| 输入码流失败 | profile/level/format 或 AnnexB/AVCC 不匹配 |

不要写“硬解成功”，除非你有实际 decoder、输入码流、命令输出和 driver/工具证据。

## 9. gdb 定点调试

当你已经知道哪个阶段出问题，可以用 gdb 进源码。

```bash
gdb --args ./bin/04_vm_vim2m_queue_loop --device=/dev/video0
```

常用命令：

```gdb
break main
run
bt
info locals
next
step
continue
```

如果要看 ioctl 前后的参数，可以在源码里的 `qbuf`、`dqbuf`、`stream_on`、`stream_off` 相关函数附近按文件行号下断点。调试符号必须存在；如果你用 `-O2` 编译，局部变量可能被优化掉。

## 10. 一份工作化 debug report 模板

每次遇到问题，按这个格式写报告：

```text
标题：
环境：
  VM/RK：
  内核版本：
  设备节点：
  FFmpeg/GStreamer 版本：
  输入文件/格式：

复现命令：

期望结果：

实际结果：

关键指标：
  CPU：
  elapsed/user/system time：
  RSS/VSZ：
  fd 数量：
  mmap/munmap 是否对称：
  qbuf/dqbuf counters：
  timeout_count：
  bytesused：

关键日志：
  strace 摘要：
  metrics 摘要：
  dmesg 摘要：

初步分层：
  user-space / framework / V4L2 queue / driver / power / hardware / input bitstream

当前判断：

下一步动作：
```

## 11. Stage06 调试验收

你完成本教程，不是因为把每条命令都背下来，而是因为你能独立回答：

1. CPU 高时，是忙等、系统调用频繁、copy 热点，还是硬件路径 fallback？
2. 耗时高时，是 `poll` 等待、DQBUF 不返回，还是软件解码路径慢？
3. 内存涨时，如何区分一次性分配和持续泄漏？
4. fd/mmap 是否有对称清理证据？
5. `QBUF/DQBUF/STREAMON/STREAMOFF` 是否形成完整 queue lifecycle？
6. VM `vim2m` 证据和 RK 硬件证据分别能证明什么，不能证明什么？
