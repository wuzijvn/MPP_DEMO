# Stage03 代码走读（完整版）

## 代码阅读顺序

1. `include/00_m2m_demo_common.hpp`
2. `src/04_try_set_format.cpp`
3. `src/05_request_query_mmap_buffers.cpp`
4. `src/06_qbuf_dqbuf_ownership.cpp`
5. `src/07_streamon_streamoff.cpp`
6. `src/08_poll_timeout.cpp`
7. `src/10_source_change_eos_drain_note.cpp`
8. `src/11_bitstream_payload_to_qbuf_bytesused.cpp`

## 相比上一个 demo 的递进关系

1. 04 新增：双队列 S_FMT。
2. 05 新增：REQBUFS + QUERYBUF + MMAP。
3. 06 新增：QBUF/DQBUF ownership loop + poll。
4. 07 新增：聚焦 STREAMON/OFF 对称性。
5. 08 新增：timeout 观测。
6. 10 新增：SOURCE_CHANGE/EOS/drain 恢复清单。
7. 11 新增：Annex B NALU 到 OUTPUT `bytesused` 的桥接说明。

## 关键结构体与函数

| 名称 | 类型 | 角色 | 生命周期 | 常见错误 | 驱动影子线 |
| --- | --- | --- | --- | --- | --- |
| `MappedBuffer` | struct | 保存 mmap 结果 | querybuf_map 后有效 | 忘记 munmap | 映射 vb2 分配 buffer |
| `set_format` | func | 配置队列格式 | 每次配置调用 | fourcc 无效 | 对应 vidioc_s_fmt |
| `reqbufs` | func | 申请缓冲池 | S_FMT 后调用 | count 太大被拒绝 | 对应 vb2 queue_setup |
| `querybuf_map_single_planar` | func | 建立用户态映射 | reqbufs 后调用 | offset/length 误解 | 对应 QUERYBUF + mmap |
| `qbuf_single_planar` | func | 用户态交还 buffer | streamon 前后均可 | bytesused 不合理 | ownership: user->driver |
| `dqbuf_single_planar` | func | 驱动归还 buffer | poll 后调用 | EAGAIN 处理不足 | ownership: driver->user |
| `poll_readable` | func | 等待设备事件 | 循环调用 | timeout 判定错误 | 驱动 ready/中断完成映射 |
| `parse_annexb_nalus` | func | 扫描 Annex B start code | 输入文件读入后 | MP4/AVCC 不含 start code | 驱动收到 OUTPUT payload 前的用户态准备 |
| `print_qbuf_plan_for_nalu` | func | 展示 `bytesused` 规划 | 每个 NALU 打印一次 | bytesused 截断/过大 | 决定驱动本次消费字节范围 |

## 资源生命周期

1. `open_node` 申请 fd。
2. `set_format` 完成格式协商。
3. `reqbufs` 建缓冲池。
4. `querybuf_map` 建映射。
5. `qbuf/dqbuf` 发生所有权循环。
6. `stream_off` 停流。
7. `unmap_all` 回收映射。
8. `close(fd)` 释放设备句柄。

demo11 是离线 payload 规划工具，不进入上面的设备生命周期。它的资源生命周期是：读取文件或内置样本 -> 扫描 NALU -> 打印 qbuf_plan -> 退出。

## 错误路径要点

1. 任一步 ioctl 失败，立即进入对称清理。
2. 映射失败时要回收已映射的所有 buffer。
3. STREAMON 半成功时要回滚已开启队列。

## 工作中真实场景映射

1. bring-up：先 04/05 确认接口可达。
2. 稳定性：06/08 看 DQBUF 与 timeout。
3. 恢复路径：10 用于评审 SOURCE_CHANGE/EOS/drain 方案。
4. 真实码流接入前置学习：11 用于解释 OUTPUT `bytesused` 为什么不能继续用占位值。
