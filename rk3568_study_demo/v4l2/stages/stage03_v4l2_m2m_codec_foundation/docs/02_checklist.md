# Stage03 快速验收（编号规范版）

## 必跑命令

```bash
./scripts/run_01_open_video_device.sh
./scripts/run_02_querycap_enum_formats.sh
./scripts/run_03_two_queue_sequence_sim.sh
./scripts/run_04_try_set_format.sh
./scripts/run_05_request_query_mmap.sh
./scripts/run_06_qbuf_dqbuf_ownership.sh
./scripts/run_07_streamon_streamoff.sh
./scripts/run_08_poll_timeout.sh
./scripts/run_09_m2m_decoder_sequence_pseudocode.sh
./scripts/run_10_source_change_eos_drain_note.sh
./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh
```

## 关键通过点

1. 01：能 open 节点。
2. 02：能拿到 QUERYCAP 与格式列表。
3. 03：能看清所有权模拟顺序，输出在 `logs/sim_sequence/`。
4. 04~07：S_FMT/REQBUFS/MMAP/QBUF/DQBUF/STREAMON/OFF 关键路径可达。
5. 08：能解释 timeout 或事件返回。
6. 10：能复述 SOURCE_CHANGE 与 EOS/drain 恢复步骤。
7. 11：能解释 NALU 范围、payload 长度、OUTPUT `bytesused` 的关系。
