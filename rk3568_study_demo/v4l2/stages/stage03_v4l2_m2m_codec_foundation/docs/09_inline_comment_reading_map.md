# Stage03 函数内注释阅读索引（按你要求的超细颗粒度）

## 目标

这份索引专门回答：
1. 函数内部注释在哪里看。
2. 哪些代码块写了“输入假设/状态变化/失败路径/驱动影子线”。
3. 哪些地方明确写了所有权方向。

## 先看公共头（关键 API 注释主入口）

文件：`include/00_m2m_demo_common.hpp`

建议阅读顺序：
1. `xioctl`：EINTR 重试语义、ioctl 入口映射。
2. `open_node/querycap`：open 与 QUERYCAP 基础行为。
3. `set_format`：S_FMT 参数填充、单平面/多平面分支、失败语义。
4. `reqbufs/querybuf_map_single_planar`：vb2 分配与 mmap 生命周期。
5. `qbuf_single_planar/dqbuf_single_planar`：所有权方向（user->driver / driver->user）。
6. `stream_on/stream_off_best_effort`：状态切换与回滚思路。
7. `poll_readable`：timeout/event/error 三态。
8. `unmap_all`：资源对称回收。

## 每个 demo 的函数内注释重点

### `src/01_open_video_device.cpp`
1. 参数解析分支注释。
2. `main` 中 open/close 对称释放说明。

### `src/02_querycap_enum_formats.cpp`
1. `enum_one_type` 中 ENUM_FMT 循环终止条件说明。
2. `main` 中 capability 选择逻辑与 OUTPUT/CAPTURE 枚举意图说明。

### `src/03_two_queue_sequence_sim.cpp`
1. `main` 中 DQBUF/QBUF 所有权方向可视化注释。
2. 报告文件写出与失败处理注释。

### `src/04_try_set_format.cpp`
1. `main` 中 S_FMT OUTPUT 再 CAPTURE 的顺序注释。
2. 每个失败分支为何立即 close 退出。

### `src/05_request_query_mmap_buffers.cpp`
1. S_FMT -> REQBUFS 前置关系注释。
2. OUTPUT/CAPTURE 两段 QUERYBUF+MMAP 的失败清理注释。
3. 成功路径对称 unmap 注释。

### `src/06_qbuf_dqbuf_ownership.cpp`
1. CAPTURE 先 QBUF 的前置条件注释。
2. OUTPUT 占位 payload 注释（明确不是功能解码证明）。
3. poll 三态分支（timeout/error/event）注释。
4. DQBUF/QBUF 5 次所有权转移逐段注释。
5. 结束清理路径对称性注释。

### `src/07_streamon_streamoff.cpp`
1. 仅验证启停、不进入主循环的边界说明。
2. STREAMON 失败时半成功回滚注释。

### `src/08_poll_timeout.cpp`
1. poll 结果三分支解释注释。
2. timeout 与事件返回各自含义注释。

### `src/09_m2m_decoder_sequence_pseudocode.cpp`
1. 文件边界注释（伪代码打印器，不执行真实 ioctl）。

### `src/10_source_change_eos_drain_note.cpp`
1. SOURCE_CHANGE 八步重配注释。
2. EOS/drain 五步收敛注释。
3. main 中 poll 观察与恢复清单关系注释。

### `src/11_bitstream_payload_to_qbuf_bytesused.cpp`
1. 文件边界注释（payload 规划，不执行真实 QBUF）。
2. `read_file` 中完整读取与失败路径注释。
3. `parse_annexb_nalus` 中 start code 扫描和 NALU 范围注释。
4. `parse_nalu_type` 中 H.264/H.265 type 差异和 stateful/stateless 影子线注释。
5. `print_qbuf_plan_for_nalu` 中 `bytesused`、截断、脏数据风险注释。

## 你最关心的三类信息现在在哪

1. 函数内部注释：
- 已在 `include` 与 `src/01~11` 的关键分支补齐（不只函数头）。

2. 所有权方向：
- 重点在 `qbuf_single_planar/dqbuf_single_planar`（公共头）和 `src/06` 主循环。
- `src/11` 负责解释真实 payload 进入 OUTPUT buffer 前，`bytesused` 应如何规划。

3. 错误路径清理：
- 重点在 `src/05`、`src/06`、`src/07` 的失败回滚与对称释放注释。

## 已知边界（避免误解）

1. 当前 `src/06` 仍是占位 payload，证明的是状态机路径，不是完整真实码流解码。
2. 当前 `src/11` 补齐 payload 规划，但仍不调用 `VIDIOC_QBUF`。
3. 若要验证真实 bitstream 解码，需要后续独立 demo：读取真实 Annex B -> OUTPUT QBUF -> CAPTURE DQBUF -> frame 校验。
