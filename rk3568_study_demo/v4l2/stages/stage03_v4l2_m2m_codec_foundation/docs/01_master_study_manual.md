# Stage03 总手册：V4L2 M2M 完整工作流（深度版）

## 阶段目标

用一套可执行 demo 完整吃透 V4L2 M2M stateful decoder 的关键链路：
`S_FMT -> REQBUFS -> QUERYBUF/MMAP -> QBUF/DQBUF -> STREAMON/OFF -> timeout 观察 -> SOURCE_CHANGE/EOS/drain 恢复策略`。

## 你会做什么

1. 运行 `01/02` 识别可训练节点与格式能力。
2. 跑 `01~11` 完整拆分 demo（核心实操聚焦 `04~08 + 10~11`）。
3. 用实验矩阵做正常/故障/恢复/性能观察。
4. 输出日志证据和阶段结论。

## 你会学到什么知识

1. decoder OUTPUT/CAPTURE 的严格方向语义。
2. buffer ownership 在 QBUF/DQBUF 间如何流转。
3. `VIDIOC_QUERYBUF + mmap` 为什么是关键生命周期。
4. poll timeout 与状态机卡住的关系。
5. SOURCE_CHANGE/EOS/drain 为什么是真实项目高频 bug 点。
6. Annex B NALU payload 如何变成 OUTPUT QBUF 的 `bytesused`。
7. 如何把基础知识点收敛到企业级服务骨架（状态机、日志、指标、门禁、故障注入）。

## 驱动影子线：这一阶段对应的驱动侧知识

1. `VIDIOC_S_FMT` 对应格式协商回调；驱动可能改写尺寸/stride。
2. `VIDIOC_REQBUFS/QUERYBUF` 对应 videobuf2 分配与缓冲元数据建立。
3. `VIDIOC_QBUF/DQBUF` 对应 buffer ownership 在用户态和驱动态之间转移。
4. `VIDIOC_STREAMON/OFF` 对应队列状态机启停。
5. `V4L2_EVENT_SOURCE_CHANGE` 对应解码分辨率变化时 CAPTURE 重配触发。
6. EOS/drain 对应驱动输出收敛与最后帧回收逻辑。
7. OUTPUT `bytesused` 决定驱动本次消费的有效码流范围，错误值可能导致解析失败、timeout 或 decode error。
8. 企业级 pipeline 需要在“可运行”之外，确保“可观测、可复盘、可验收”。

## 对应岗位能力

1. 能独立写出最小 M2M bring-up 流程。
2. 能对 `STREAMON` / `DQBUF` 失败做层次化定位。
3. 能与驱动同学对齐 SOURCE_CHANGE 与 drain 的处理契约。

## 动手任务（必须可执行）

1. `./scripts/run_04_try_set_format.sh`
2. `./scripts/run_05_request_query_mmap.sh`
3. `./scripts/run_06_qbuf_dqbuf_ownership.sh`
4. `./scripts/run_07_streamon_streamoff.sh`
5. `./scripts/run_08_poll_timeout.sh`
6. `./scripts/run_10_source_change_eos_drain_note.sh`
7. `./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh`
8. `./scripts/run_all_stage03.sh`
9. `./scripts/run_12_enterprise_m2m_pipeline_service.sh`

## 验收标准（通过/不通过）

通过：
1. 六个 demo 中至少 04/05/07 在当前节点可达并成功。
2. 06 能给出 `dq_cap_ok/dq_out_ok/poll_timeouts` 指标（即使未持续成功也能解释原因）。
3. 能复述 SOURCE_CHANGE/EOS/drain 恢复顺序。
4. 能解释 demo06 占位 payload 与 demo11 真实/教学 payload 规划的区别。

不通过：
1. 把 OUTPUT/CAPTURE 方向讲反。
2. 不能解释 QBUF/DQBUF ownership 方向。
3. 遇到 timeout 无法给出下一步排查动作。

## 常见坑

1. 只设 OUTPUT 不设 CAPTURE 就 STREAMON。
2. 忘记先 QBUF CAPTURE 空 buffer。
3. STREAMOFF/munmap/close 清理不对称。
4. 把虚拟驱动成功当作硬件性能成功。
5. 把 demo11 的 payload 规划当成真实硬解成功证明。
6. 只看“程序返回 0”，却不看 enterprise 指标 JSON 与状态机迁移证据。

## 面试表达模板

“我在 Stage03 用拆分 demo 跑通了 V4L2 M2M 关键状态机：S_FMT、REQBUFS、QUERYBUF/MMAP、QBUF/DQBUF、STREAMON/OFF，并专门做了 poll timeout、SOURCE_CHANGE/EOS/drain 恢复路径和 Annex B payload 到 OUTPUT `bytesused` 的桥接训练。能把每个用户态步骤映射到 vb2、驱动状态机和码流输入契约。”

## 本阶段总结：通过这些例子你学到了什么

1. 核心知识：双队列状态机 + buffer 生命周期 + 恢复路径 + payload/bytesused 边界。
2. 驱动影子线：S_FMT/REQBUFS/QUERYBUF/QBUF/DQBUF/STREAMON/EVENT/bytesused 的驱动映射。
3. 岗位映射：具备 codec bring-up 与基础排障表达能力。
4. 可独立完成：最小工作流验证、日志收集、失败分层定位。
5. 剩余缺口：demo11 已补 payload 规划，但真实 QBUF 解码、CAPTURE frame 校验与完整 SOURCE_CHANGE 实测仍需后续 stage 继续深化。
6. 企业级补充项目已提供工程化骨架；后续可继续接入真实码流输入和帧级校验把它从“服务骨架”推进到“可验画质/性能”的准生产链路。
