# Stage03 - V4L2 M2M Codec Workflow Full Package (Normalized)

## 当前环境定位

Stage03 现在定位为 `VM 优先` 的 V4L2 M2M 逻辑训练阶段。

1. 在 VM 上：如果有虚拟 V4L2 M2M 节点，可以用来验证 ioctl 顺序、OUTPUT/CAPTURE 双队列、buffer ownership、poll/timeout 等用户态状态机逻辑。
2. 在 RK 板上：当前不要求真实 V4L2 M2M codec decode 成功。RK 板真实硬解主线放到 Stage05 RKMPP，例如 FFmpeg `h264_rkmpp` / `hevc_rkmpp`。
3. VM 跑通 Stage03 不等于证明实体 VPU 硬解；它证明的是你掌握了 Linux codec M2M 的通用工作语言。

完整双环境路线见：

```bash
less /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_dual_environment_codec_route.md
```

## 这个 demo 教什么

按规范连续编号 `01~11`，完整覆盖 V4L2 M2M stateful decoder 学习主线：
1. `01` open 设备。
2. `02` QUERYCAP + ENUM_FMT。
3. `03` 双队列所有权模拟主流程。
4. `04` S_FMT 双队列协商。
5. `05` REQBUFS + QUERYBUF + MMAP 生命周期。
6. `06` QBUF/DQBUF ownership 循环。
7. `07` STREAMON/STREAMOFF 对称切换。
8. `08` poll timeout 观察。
9. `09` 完整状态机伪代码可运行输出。
10. `10` SOURCE_CHANGE/EOS/drain 恢复路径清单。
11. `11` Annex B 码流 payload 到 OUTPUT QBUF `bytesused` 的桥接训练。

并追加一个企业级补充项目（`12`）：
12. `12` `enterprise_project/`：把基础知识点收敛为服务化骨架（CLI + 状态机 + 指标 JSON + 故障注入矩阵 + 验收门禁）。

## 文件结构（规范化）

```text
stage03_v4l2_m2m_codec_foundation/
├── src/01_open_video_device.cpp
├── src/02_querycap_enum_formats.cpp
├── src/03_two_queue_sequence_sim.cpp
├── src/04_try_set_format.cpp
├── src/05_request_query_mmap_buffers.cpp
├── src/06_qbuf_dqbuf_ownership.cpp
├── src/07_streamon_streamoff.cpp
├── src/08_poll_timeout.cpp
├── src/09_m2m_decoder_sequence_pseudocode.cpp
├── src/10_source_change_eos_drain_note.cpp
├── src/11_bitstream_payload_to_qbuf_bytesused.cpp
├── scripts/run_12_enterprise_m2m_pipeline_service.sh
├── enterprise_project/
│   ├── src/05_enterprise_pipeline_main.cpp
│   ├── scripts/run_12_enterprise_m2m_pipeline_service.sh
│   ├── scripts/run_12_enterprise_fault_matrix.sh
│   ├── docs/12_enterprise_architecture.md
│   └── expected_output/12_enterprise_m2m_pipeline_service.txt
├── include/00_m2m_demo_common.hpp
├── scripts/run_01_*.sh ... run_11_*.sh
├── expected_output/01_*.txt ... 11_*.txt
├── logs/
└── docs/
```

## 为什么删掉 `stage03_outputs_sequence_sim`

该目录是旧版临时输出路径，不符合当前 stage 包结构规范。
已统一迁移为：
1. `logs/sim_sequence/`（03 模拟输出）
2. `logs/run_all_*/`（全流程证据）

## 编译

```bash
./build.sh all
```

## 运行

```bash
./scripts/run_all_stage03.sh
```

企业级项目单跑：

```bash
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

## 是否需要音视频样本文件

1. `01~03`、`09~10` 不需要音视频样本：它们是节点/流程/恢复路径教学 demo。
2. `04~08` 默认也不强制需要真实码流文件：当前用于训练 ioctl 状态机与队列所有权。
3. `11` 默认使用内置 Annex B 教学样本；也可以通过 `INPUT=... CODEC=h264|h265` 观察真实 elementary stream 的 NALU 与 `bytesused`。
4. 若要验证“真实解码有效性”（而不是仅流程可达），需要样本码流并扩展为真实 `OUTPUT` 投喂：
   - 推荐样本：`samples/sample_720p_h264.annexb`、`samples/sample_1080p_h265.annexb`
   - 当前默认 `06` 的占位 `bytesused` 只证明状态机路径，不证明真实编解码结果。
   - `11` 只证明 payload 规划和 `bytesused` 理解，不调用 `VIDIOC_QBUF`。

## 验收

看：`docs/02_final_checklist.md`

额外环境验收：

1. VM：能说明虚拟 M2M 节点通过代表 ioctl/state-machine 通过，不代表硬件性能通过。
2. RK 板：如果没有 M2M codec 节点，不把 Stage03 判定失败；改到 Stage06 做 reality check，再回 Stage05 跑 RKMPP 真实硬解。
