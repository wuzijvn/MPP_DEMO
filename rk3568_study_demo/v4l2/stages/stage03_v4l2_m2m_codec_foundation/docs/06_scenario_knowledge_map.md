# Stage03 场景-知识-指标映射（完整版）

## 场景1：STREAMON 失败

1. 知识点：S_FMT、REQBUFS、队列准备顺序。
2. 指标：E2/E3/E5 成败链路。
3. 指标含义：
   - E2/E3 成功但 E5 失败，优先怀疑状态切换契约。
4. 下一步：核对 queue 类型、memory 类型、驱动日志。

## 场景2：DQBUF 经常超时

1. 知识点：QBUF 先后顺序、poll 超时、输入有效性。
2. 指标：`dq_cap_ok/dq_out_ok/poll_timeouts`。
3. 指标含义：
   - `poll_timeouts` 高且 `dq_*` 低，优先查输入与队列饿死。
4. 下一步：增加 timeout、检查 bytesused、对照 dmesg。

## 场景3：分辨率变化后输出异常

1. 知识点：SOURCE_CHANGE 恢复八步。
2. 指标：是否执行了 CAPTURE 重配全流程。
3. 指标含义：
   - 缺任一步都可能导致后续 DQBUF 异常。
4. 下一步：执行 demo10 清单并对照实现代码。

## 场景4：结束收尾阶段卡住

1. 知识点：EOS/drain 收敛路径。
2. 指标：最后帧回收数量、STREAMOFF 后是否干净退出。
3. 指标含义：
   - 未 drain 直接停流可能导致最后帧丢失或状态未清。
4. 下一步：按 drain 清单补全收敛逻辑。

## 场景5：接入真实码流后仍然 DQBUF timeout

1. 知识点：Annex B NALU、OUTPUT `bytesused`、有效输入边界。
2. 指标：demo11 的 `nal_count/total_qbuf_bytes/qbuf_plan`，以及 demo06/后续真实解码 demo 的 `dq_cap_ok/poll_timeouts`。
3. 指标含义：
   - demo11 找不到 NALU，优先怀疑输入不是 Annex B。
   - `bytesused` 与实际拷贝长度不一致，可能导致驱动解析到截断码流或脏数据。
   - `nal_count` 正常但 CAPTURE 无输出，下一步看 QBUF 顺序、访问单元边界、codec header 和 dmesg。
4. 下一步：先跑 `./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh` 验证 payload 规划，再接入真实 QBUF 解码路径。
