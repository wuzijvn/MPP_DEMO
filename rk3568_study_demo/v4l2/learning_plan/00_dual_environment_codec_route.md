# 双环境 Codec 学习路线：VM 练 V4L2 M2M，RK 板验 RKMPP 硬件路径

## 一句话结论

当前学习路线正式拆成两条互补环境线：

1. `VM / x86 Linux`：用于学习 V4L2 M2M 的通用 ioctl、OUTPUT/CAPTURE 双队列、buffer ownership、SOURCE_CHANGE、EOS/drain、timeout debug。这里的虚拟 M2M 节点只能证明状态机逻辑和用户态代码路径，不证明真实 VPU 硬解性能。
2. `RK 板 / RK3568`：用于验证真实硬件编解码、性能、稳定性、dmesg/driver-facing 证据。当前真实硬解主线走 RKMPP，例如 FFmpeg `h264_rkmpp` / `hevc_rkmpp`，不要强行把 rkisp/camera 节点当作 V4L2 M2M codec 节点。

这不是路线降级，而是更接近真实 SoC 适配工作的路线：通用 Linux codec 模型在 VM 学，板端真实硬件路径在 RKMPP 验。

## 为什么要这样调整

当前事实是：

1. RK 板上 `/dev/video0~9` 更像 rkisp/camera capture 节点，不是 codec M2M 节点。
2. `/dev/video-dec0`、`/dev/video-enc0` 如果不是字符设备，就不能作为 V4L2 ioctl codec 节点。
3. FFmpeg 能看到 `h264_rkmpp`、`hevc_rkmpp` 等 decoder，说明板端真实硬解验证更应该走 RKMPP wrapper。
4. VM 中即使有虚拟 V4L2 M2M 节点，也没有实体 VPU、firmware、IRQ、DMA/IOMMU、runtime PM，所以不能证明真实硬解，只能证明 V4L2 M2M 用户态状态机。

所以新的学习策略是：

| 学习目标 | 首选环境 | 使用 stage | 证明什么 | 不证明什么 |
| --- | --- | --- | --- | --- |
| V4L2 M2M ioctl 顺序 | VM | Stage03 / Stage06 | open/querycap/S_FMT/REQBUFS/QBUF/DQBUF/STREAMON 逻辑 | 不证明 RK VPU 硬解 |
| OUTPUT/CAPTURE 双队列所有权 | VM | Stage03 / Stage06 | buffer ownership 和 queue loop | 不证明硬件吞吐 |
| SOURCE_CHANGE / EOS / drain | VM + 模拟 | Stage06 | 状态机边界和 debug 报告写法 | 不证明真实 firmware 行为 |
| FFmpeg 软件基线 | VM 或 RK 板 | Stage04 | CPU decode baseline、输入文件有效性 | 不证明硬件路径 |
| RK 板真实硬解 | RK 板 | Stage05 | `h264_rkmpp/hevc_rkmpp` 是否能解码、性能差异、fallback 证据 | 不证明 V4L2 M2M 节点存在 |
| SoC bring-up 报告 | RK 板 | Stage05 + Stage06 reality check | device nodes、dmesg、FFmpeg decoder、性能和限制 | 不把 VM 虚拟结果写成板端能力 |
| DMA-BUF / DRM PRIME / zero-copy | RK 板优先，VM 辅助概念 | 后续 Stage08/09 | 板端 buffer/display 约束和 hidden copy | VM 无法证明 RK 显示链路 |

## 新的阶段职责

### Stage03：V4L2 M2M Codec Foundation

定位：`VM 优先`。

你在 Stage03 要学的是：

1. V4L2 M2M codec 节点如何被识别。
2. decoder 为什么有 OUTPUT/CAPTURE 两个 queue。
3. OUTPUT queue 为什么放 compressed bitstream。
4. CAPTURE queue 为什么拿 decoded raw frame。
5. `S_FMT/REQBUFS/QUERYBUF/MMAP/QBUF/DQBUF/STREAMON/STREAMOFF` 的顺序和失败点。
6. `bytesused`、`sequence`、`timestamp`、`flags` 这些字段如何成为 debug 证据。

VM 验收标准：

1. 能跑 Stage03 全套 demo 或至少跑通虚拟 M2M 节点的 QUERYCAP/格式/队列相关实验。
2. 能说明“虚拟节点通过”只代表用户态状态机逻辑通过。
3. 能写出一段 stateful decoder ioctl sequence 伪代码。
4. 能解释 `STREAMON` 或 `DQBUF timeout` 的分层排查顺序。

RK 板验收标准：

1. 只做 reality check，不要求真实 V4L2 M2M decode 成功。
2. 如果没有 M2M codec 节点，结论写成 `RKMPP_REAL_PATH_PLUS_V4L2_M2M_CONCEPT`。
3. 不对 rkisp/camera 节点强行跑 codec M2M ioctl。

### Stage04：FFmpeg Software Path

定位：`VM 和 RK 板都要跑`。

Stage04 是两条环境线的共同基线：

1. VM 跑 Stage04：证明输入文件、FFmpeg API、软件 decode baseline 没问题。
2. RK 板跑 Stage04：给 Stage05 RKMPP 对比提供 CPU 软件基线。

验收标准：

1. 同一输入文件能用软件 decoder 解码。
2. 能记录 `real/user/sys`、fps 或 frame count。
3. 如果软件路径失败，先修输入/容器/codec/API，不要直接怀疑硬件。

### Stage05：FFmpeg Hardware Acceleration and RKMPP Path

定位：`RK 板优先`。

Stage05 是 RK 板真实硬件验证主线：

1. 默认使用 `h264_rkmpp` / `hevc_rkmpp`。
2. 通过 decoder selection、成功 frame count、性能对比、dmesg/日志来证明硬件路径。
3. 明确区分 `RKMPP wrapper 输出 CPU 可见帧` 和 `软件 fallback`。
4. 不把 `frame.format=yuv420p` 简单判定成软解，因为 RKMPP wrapper 可能返回 CPU 可见帧。

RK 板验收标准：

1. `ffmpeg -hide_banner -decoders | grep rkmpp` 能看到目标 decoder。
2. `h264_rkmpp/hevc_rkmpp` 至少一种能对样本成功出帧。
3. 有软件 decoder vs RKMPP decoder 的对比数据。
4. 输出 fallback 结论，至少包括 decoder 名、frame count、失败原因或 pass gate。

VM 验收标准：

1. 只学习 FFmpeg hwaccel 概念和代码结构。
2. 如果 VM 没有 RKMPP，不要求 `h264_rkmpp` 成功。
3. VM 的 VAAPI/V4L2 M2M 实验可以作为框架对比，但不写成 RK 板结论。

### Stage06：V4L2 M2M Deep Dive

定位：`VM 学真实/虚拟 M2M 逻辑，RK 板学现实适配和报告边界`。

Stage06 不再被定义为“必须在 RK 板跑真实 V4L2 M2M 硬解”。它的职责是：

1. 在 VM 上深化 V4L2 M2M stateful decoder 状态机。
2. 在 RK 板上明确没有 M2M codec 节点时的替代路径。
3. 把 debug report 写成对驱动工程师有用的证据格式。
4. 把 `V4L2 M2M 通用模型` 和 `RKMPP 板端真实路径` 做对照。

VM 验收标准：

1. 能跑基础 demo 和 enterprise fault matrix。
2. 能解释 `QBUF -> driver owns buffer -> IRQ/worker completion -> DQBUF`。
3. 能解释 source change 后为什么要重配 CAPTURE queue。
4. 能写 DQBUF timeout debug report。

RK 板验收标准：

1. 必跑 `scripts/run_00_rk_board_reality_check.sh`。
2. 如果输出 `v4l2_m2m_status=NOT_FOUND` 且 `rkmpp_status=AVAILABLE`，Stage06 判定为概念/报告训练通过，不要求真实 M2M decode。
3. 真实硬解继续回到 Stage05 RKMPP。

## 推荐执行顺序

### 第 1 轮：统一基础

1. Stage04 在 VM 跑软件路径，确认输入和 FFmpeg API。
2. Stage04 在 RK 板跑软件路径，记录 CPU baseline。
3. Stage05 在 RK 板跑 RKMPP，记录硬件路径和软件对比。

### 第 2 轮：V4L2 M2M 逻辑

1. Stage03 在 VM 跑 V4L2 M2M 基础 demo。
2. Stage06 在 VM 跑 deep dive 和 fault matrix。
3. 把 VM 的日志写成“V4L2 M2M 逻辑学习证据”，不要写成“RK 硬件证据”。

### 第 3 轮：RK 板真实 bring-up

1. Stage06 在 RK 板跑 reality check。
2. Stage05 在 RK 板跑 `h264_rkmpp/hevc_rkmpp` benchmark。
3. 收集 dmesg、FFmpeg verbose/debug、性能对比。
4. 写 Codec Bring-up Report：明确 backend 是 RKMPP，不是 V4L2 M2M。

### 第 4 轮：后续扩展

1. GStreamer：在 RK 板优先找 RKMPP 相关插件或 vendor decoder element。
2. DRM/DMA-BUF：在 RK 板验证真实 `/dev/dri/*`、format、stride、modifier、copy count。
3. Driver shadow：用 V4L2 M2M 的通用队列语言理解 driver buffer lifecycle，用 RKMPP 证据理解板端真实工作链路。

## 每次实验报告必须标注环境

每份报告开头都写：

```text
environment=VM|RK_BOARD
backend=software|v4l2_m2m_virtual|rkmpp|vaapi|drm|simulation
hardware_proof=yes|no
what_this_proves=...
what_this_does_not_prove=...
```

示例 1：VM 跑 Stage03

```text
environment=VM
backend=v4l2_m2m_virtual
hardware_proof=no
what_this_proves=V4L2 M2M ioctl sequence and queue ownership are understood
what_this_does_not_prove=RK3568 VPU hardware decode performance or RKMPP availability
```

示例 2：RK 板跑 Stage05

```text
environment=RK_BOARD
backend=rkmpp
hardware_proof=yes
what_this_proves=FFmpeg can use h264_rkmpp/hevc_rkmpp path on this board
what_this_does_not_prove=A generic V4L2 M2M codec node exists on this board
```

## 调整后的验收总表

| 能力 | 通过标准 | 主要环境 | 主要 stage |
| --- | --- | --- | --- |
| FFmpeg 软件 baseline | 同输入可稳定软件解码并记录指标 | VM + RK | Stage04 |
| V4L2 M2M 状态机 | 能解释并运行 ioctl/queue/source-change 训练 | VM | Stage03 + Stage06 |
| RK 真实硬解 | `*_rkmpp` 成功出帧并有软件对比 | RK | Stage05 |
| RK 板现实判断 | 能证明无 M2M codec 节点时不误用 camera/ISP | RK | Stage06 reality check |
| Debug report | 能分清 command/framework/device/buffer/driver/power 层 | VM + RK | Stage06 + Stage10 |
| Performance report | 有 software vs RKMPP 数据、CPU/real/user/sys 或 fps | RK | Stage05 + Stage11 |
| Driver-shadow 表达 | 能把 QBUF/DQBUF 映射到 vb2/driver ownership | VM 概念 + RK 证据 | Stage03/06 |

## 面试/入职表达模板

> 我会把 V4L2 M2M 和 RKMPP 分开验证：V4L2 M2M 是 Linux codec 的通用 ioctl 和双队列模型，我在 VM 的虚拟 M2M 节点上练状态机、QBUF/DQBUF、SOURCE_CHANGE、EOS/drain 和 timeout debug；但我不会把 VM 结果当作实体 VPU 硬解证据。RK3568 板端如果没有真实 V4L2 M2M codec 字符设备，我会走 FFmpeg `h264_rkmpp/hevc_rkmpp` 验证真实硬件路径，并用 decoder selection、frame count、软件/硬件 benchmark、dmesg 和日志来证明是否命中硬件。这样既掌握通用 Linux codec 模型，也尊重板端 vendor backend 的真实实现。

## 常见坑

1. 把 `/dev/video0` 默认当 codec 节点。正确做法：先看 `QUERYCAP` 和 `v4l2-ctl --list-devices`。
2. 把 VM 虚拟 M2M 节点当成硬件加速证据。正确做法：VM 只证明逻辑，不证明 VPU。
3. 把 RKMPP wrapper 输出 CPU 可见帧误判为软件 fallback。正确做法：结合 decoder 名、frame count、性能和日志。
4. 看到 `ffmpeg -hwaccels` 有某后端就认为可用。正确做法：必须跑实际命令和 gate。
5. 强行对 rkisp/camera 节点跑 codec ioctl。正确做法：没有 M2M capability 就切回 Stage06 模拟和 Stage05 RKMPP。

## 下一步执行建议

1. 在 VM：跑 Stage03 `./scripts/run_all_stage03.sh`，输出一份 `vm_v4l2_m2m_logic_report.md`。
2. 在 RK 板：跑 Stage06 `./scripts/run_00_rk_board_reality_check.sh`，确认板端现实。
3. 在 RK 板：跑 Stage05 `INPUT=./samples/sample.mp4 MAX_FRAMES=120 LOOPS=5 ./scripts/run_10_performance_diagnosis_playbook.sh`，输出 RKMPP 真实性能对比。
4. 把两份报告合并成一份 `codec_backend_adaptation_report.md`：VM 负责通用模型，RK 负责真实硬件。
