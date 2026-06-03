# Stage03 从这里开始（完整执行顺序）

## 0. 先确认你在哪个环境

Stage03 推荐先在 VM 跑，因为 VM 上的虚拟 V4L2 M2M 节点适合训练 codec ioctl 和双队列逻辑。

| 环境 | 这一阶段要做什么 | 不要得出什么结论 |
| --- | --- | --- |
| VM | 跑 Stage03 demo，理解 `OUTPUT/CAPTURE`、`QBUF/DQBUF`、`STREAMON/STREAMOFF` | 不要声称 RK VPU 硬解已验证 |
| RK 板 | 可做节点识别和 reality check | 不要强行把 rkisp/camera 节点当 codec M2M |

完整路线：

```bash
less /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_dual_environment_codec_route.md
```

## 1. 编译全部 demo

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage03_v4l2_m2m_codec_foundation
./build.sh all
```

## 2. 先做环境与节点体检

```bash
./scripts/collect_env.sh
./scripts/run_01_open_video_device.sh
./scripts/run_02_querycap_enum_formats.sh
```

## 3. 依次执行 Stage03 核心 demo

```bash
./scripts/run_04_try_set_format.sh
./scripts/run_05_request_query_mmap.sh
./scripts/run_06_qbuf_dqbuf_ownership.sh
./scripts/run_07_streamon_streamoff.sh
./scripts/run_08_poll_timeout.sh
./scripts/run_10_source_change_eos_drain_note.sh
./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

## 4. 一键跑全套（推荐）

```bash
./scripts/run_all_stage03.sh
```

## 5. 验收关键点

1. 04：S_FMT 双队列成功。
2. 05：REQBUFS/QUERYBUF/MMAP 全流程成功。
3. 06：看到 QBUF/DQBUF ownership 循环统计。
4. 07：STREAMON/STREAMOFF 对称成功。
5. 08：能观察到 poll timeout 或事件返回。
6. 10：能复述 SOURCE_CHANGE/EOS/drain 的恢复顺序。
7. 11：能说明真实 payload 的有效字节数如何映射到 OUTPUT QBUF `bytesused`。
8. 12：能读懂企业级项目状态机日志与 metrics JSON，并完成至少一次故障注入验证。

## 6. 输出证据

1. `logs/run_all_*/` 下各 demo 日志。
2. `logs/run_all_*/env/` 下环境与 dmesg 抓取。
3. 对照 `docs/02_final_checklist.md` 打勾。
4. 码流桥接学习对照 `docs/10_bitstream_payload_bridge.md`。
5. 企业级项目对照 `enterprise_project/docs/12_enterprise_verification_guide.md`。

报告开头必须写：

```text
environment=VM|RK_BOARD
backend=v4l2_m2m_virtual|simulation|rkmpp|software
hardware_proof=yes|no
what_this_proves=...
what_this_does_not_prove=...
```
