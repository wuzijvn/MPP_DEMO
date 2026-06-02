# Stage03 实验矩阵（完整覆盖版）

| 实验ID | 场景类型 | 实验目标 | 验证知识点 | 命令 | 观察指标 | 通过标准 | 失败定位层级 | 驱动影子线 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| E1 | normal | 节点体检 | open/QUERYCAP/ENUM_FMT 可达性 | `./scripts/run_01_open_video_device.sh && ./scripts/run_02_querycap_enum_formats.sh` | open 成功、capability、fmt 列表 | open/querycap/enum_fmt 成功 | 设备节点层 | capability/enum_fmt |
| E2 | normal | 双队列 S_FMT | OUTPUT/CAPTURE 格式协商 | `run_04_try_set_format.sh` | S_FMT 成功率 | 双队列 S_FMT 成功 | 格式协商层 | vidioc_s_fmt |
| E3 | normal | mmap 生命周期 | REQBUFS/QUERYBUF/MMAP | `run_05_request_query_mmap.sh` | granted count、map 成功数 | 全部 map 成功 | vb2/内存层 | vb2 queue + mmap |
| E4 | normal | ownership 循环 | QBUF/DQBUF/poll | `run_06_qbuf_dqbuf_ownership.sh` | dq_cap_ok/dq_out_ok/timeouts | 至少有可解释统计输出 | 队列状态机层 | ownership 转移 |
| E5 | normal | 启停切换 | STREAMON/OFF 对称性 | `run_07_streamon_streamoff.sh` | streamon/off 结果 | 双队列启停成功 | 驱动状态层 | stream state |
| E6 | performance-observe | timeout 观测 | poll 行为 | `run_08_poll_timeout.sh` | poll ret/revents | timeout 或 event 可解释 | 调度/就绪层 | 中断/ready 唤醒 |
| E7 | recovery | 恢复路径训练 | SOURCE_CHANGE/EOS/drain | `./scripts/run_10_source_change_eos_drain_note.sh` | 恢复步骤覆盖度 | 可复述完整顺序 | 恢复策略层 | event/drain 协议 |
| E8 | fault-injection | 格式错误注入 | S_FMT 失败形态 | E2 改 `IN_FOURCC=ZZZZ` | errno/失败点 | 预期失败且定位正确 | 参数层/协商层 | 驱动拒绝不支持格式 |
| E9 | normal | payload 桥接 | Annex B NALU 到 OUTPUT `bytesused` | `./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh` | nal_count/total_qbuf_bytes/qbuf_plan | 能解释每个 NALU 的 bytesused | 码流输入层 | 驱动/固件消费 OUTPUT payload |

## 每个实验的指标意义

1. `dq_cap_ok` 增长：说明 CAPTURE 有输出回收。
2. `dq_out_ok` 增长：说明 OUTPUT 输入被消费归还。
3. `poll_timeouts` 增加：可能是无数据、队列饿死、状态机卡住或环境空闲。
4. `nal_count` 增长：说明输入中识别到更多 Annex B NALU，但不代表解码成功。
5. `total_qbuf_bytes`：说明计划交给 OUTPUT 队列的有效码流字节总量，不是吞吐性能指标。

## 证据模板

每个实验请保存：
1. 命令全文。
2. 关键输出。
3. 指标值。
4. 通过/不通过。
5. 失败层级与下一步动作。
