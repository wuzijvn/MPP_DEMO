# Stage03 API / Struct 深挖（完整覆盖）

## v4l2_format + VIDIOC_S_FMT

1. 概念角色：双队列格式协商入口。
2. 参数语义：`type` 决定 OUTPUT/CAPTURE；`pixelformat` 决定码流/帧格式。
3. 生命周期：临时栈对象，调用后可读取驱动回填值。
4. 状态机关系：REQBUFS 前必须完成。
5. 失败路径：EINVAL 常见于格式或分辨率不支持。
6. 性能影响：格式选择影响后续拷贝和带宽。
7. 驱动映射：vidioc_s_fmt_*。
8. 调试抓手：`v4l2-ctl --list-formats-ext` + dmesg。
9. 面试表达：
   - “S_FMT 是能力协商，不是单向设置。”

## v4l2_requestbuffers + VIDIOC_REQBUFS

1. 概念角色：建立 buffer 池契约。
2. 参数语义：`count`、`memory`、`type`。
3. 生命周期：成功后由驱动维护 queue 资源。
4. 状态机关系：S_FMT 后、QUERYBUF 前。
5. 失败路径：资源不足、memory 不支持。
6. 性能影响：count 影响 in-flight 深度。
7. 驱动映射：vb2 queue_setup/alloc。
8. 调试抓手：granted count 与错误码。
9. 面试表达：
   - “REQBUFS 决定了后续队列并发深度上限。”

## v4l2_buffer + VIDIOC_QUERYBUF/QBUF/DQBUF

1. 概念角色：单个 buffer 元数据与状态转移载体。
2. 参数语义：`index`、`bytesused`、`sequence`、`flags`。
3. 生命周期：
   - QUERYBUF：获取映射参数。
   - QBUF：交给驱动。
   - DQBUF：驱动归还。
4. 状态机关系：QBUF 与 DQBUF 构成循环核心。
5. 失败路径：队列空、状态不对、超时。
6. 性能影响：QBUF/DQBUF 节奏影响吞吐与延迟。
7. 驱动映射：vb2 buffer 状态机迁移。
8. 调试抓手：bytesused/sequence/poll 行为。
9. 面试表达：
   - “QBUF 是 ownership 交付，DQBUF 是 ownership 回收。”

## poll

1. 概念角色：等待设备就绪或事件。
2. 参数语义：timeout_ms 控制等待窗口。
3. 状态机关系：通常位于 DQBUF 前。
4. 失败路径：ret<0 表示错误，ret=0 表示 timeout。
5. 性能影响：timeout 设置过短会增加空转。
6. 驱动映射：中断/完成事件唤醒等待队列。
7. 调试抓手：revents、timeout 频率。
8. 面试表达：
   - “poll timeout 是定位队列卡住和输入不足的重要证据。”

## SOURCE_CHANGE/EOS/drain（恢复协议）

1. 概念角色：处理动态分辨率变化与收尾阶段。
2. 状态机关系：属于主循环中的分支路径。
3. 失败路径：漏重配或漏 drain 造成后续异常。
4. 驱动映射：事件上报、尾帧收敛策略。
5. 面试表达：
   - “SOURCE_CHANGE 要先停 CAPTURE 重配再恢复，EOS/drain 要等尾帧收敛。”

## Annex B NALU + OUTPUT bytesused

1. 概念角色：把压缩 elementary stream 切成可投喂 OUTPUT queue 的有效字节范围。
2. 参数/字段语义：
   - `start code`：Annex B NALU 边界，通常是 `00 00 01` 或 `00 00 00 01`。
   - `NALU type`：H.264 中 SPS=7、PPS=8、IDR=5；H.265 中 VPS/SPS/PPS 使用不同 type 编码。
   - `bytesused`：本次 OUTPUT QBUF 中真实有效码流字节数，不是 buffer 总容量。
3. 生命周期：用户态读取/切分码流，拷入 OUTPUT mmap buffer，QBUF 后所有权交给驱动，DQBUF 后输入 buffer 才可复用。
4. 状态机关系：通常发生在 `REQBUFS/QUERYBUF/MMAP` 之后、`VIDIOC_QBUF(OUTPUT)` 之前。
5. 失败路径：非 Annex B 输入、SPS/PPS/VPS 缺失、bytesused 截断、bytesused 过大、访问单元边界不符合驱动预期。
6. 性能影响：payload 分块过碎会增加 ioctl 次数；过大可能超过 buffer 容量或增加延迟。
7. 驱动映射：stateful 驱动/固件可能内部解析 header；stateless 路径通常需要用户态解析并通过 controls/request API 传参。
8. 调试抓手：demo11 的 `qbuf_plan`、实际 QBUF `bytesused`、DQBUF timeout、dmesg decode error。
9. 面试表达：
   - “OUTPUT QBUF 的 bytesused 是本次交给驱动消费的有效码流长度；它错了，后面的 timeout 或 decode error 就不一定是驱动 bug。”
