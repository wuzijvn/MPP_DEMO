# Stage03 最终验收清单（完整版）

## A. 构建

- [ ] `./build.sh all` 成功。
- [ ] `bin/04_* 05_* 06_* 07_* 08_* 10_* 11_*` 均存在。
- [ ] `enterprise_project/build.sh` 成功，且 `enterprise_project/bin/12_enterprise_m2m_pipeline_service` 存在。

## B. 功能性证据

- [ ] `run_01_open_video_device.sh` 与 `run_02_querycap_enum_formats.sh` 有结果（作为 preflight）。
- [ ] demo04 通过或给出明确失败原因。
- [ ] demo05 通过或给出明确失败原因。
- [ ] demo06 产出循环统计。
- [ ] demo07 启停对称。
- [ ] demo08 看到 timeout 或 event。
- [ ] demo10 能打印恢复路径。
- [ ] demo11 能打印 NALU 列表和 `qbuf_plan`。
- [ ] demo12（enterprise）能输出 `enterprise_pipeline.log` 和 `enterprise_metrics.json`。

## C. 指标证据

- [ ] 记录 `dq_cap_ok/dq_out_ok/poll_timeouts`。
- [ ] 记录至少一次 fault-injection（如 `IN_FOURCC=ZZZZ`）。
- [ ] 记录 demo11 的 `nal_count/total_qbuf_bytes`，并说明它不是硬解性能指标。
- [ ] 记录 demo12 的 `state_transition/qbuf_out/dqbuf_eagain/poll_timeout/source_change` 并解释趋势。

## D. 解释性证据

- [ ] 能解释 OUTPUT/CAPTURE 方向。
- [ ] 能解释 QBUF/DQBUF 所有权方向。
- [ ] 能解释 SOURCE_CHANGE 八步重配。
- [ ] 能解释 EOS/drain 五步收敛。
- [ ] 能解释 `bytesused` 为什么必须等于本次有效码流字节数。
- [ ] 能解释 enterprise gate 为什么 PASS/FAIL（不是只看一个指标）。

## E. 驱动影子线证据

- [ ] 能把 S_FMT/REQBUFS/QUERYBUF/QBUF/DQBUF/STREAMON 映射到驱动概念。
- [ ] 能把 OUTPUT payload、SPS/PPS/VPS、stateful/stateless 解析责任映射到驱动概念。
- [ ] 失败时能分层（命令层/框架层/节点层/驱动层）。
- [ ] 能说明 enterprise 项目中注入型 SOURCE_CHANGE 与真实内核事件的边界。

## F. 通过门槛

满足 A+B+C+D+E 即判定 Stage03 通过。
