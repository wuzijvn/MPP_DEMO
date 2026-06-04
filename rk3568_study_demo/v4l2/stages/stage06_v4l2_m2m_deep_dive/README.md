# Stage06 - V4L2 M2M Deep Dive

<<<<<<< HEAD
## 当前环境定位

Stage06 是“双环境桥接阶段”：

1. VM：深化 V4L2 M2M stateful decoder 状态机，训练 ioctl sequence、queue ownership、poll timeout、SOURCE_CHANGE、EOS/drain、fault matrix。
2. RK 板：先做 reality check。如果没有 V4L2 M2M codec 字符设备，Stage06 不要求真实 M2M decode；真实硬解验证回到 Stage05 RKMPP。
3. 本阶段的核心产出是“你能分清环境和 backend，并写出证据边界清楚的 debug report”。

完整双环境路线见：

```bash
less /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_dual_environment_codec_route.md
```

## RK 板端现实适配
=======
Stage06 是 Stage03 V4L2 入门的深化版。Stage03 主要回答“这个 `/dev/videoX` 是什么、能不能 `QUERYCAP/ENUM_FMT`”；Stage06 继续往下走到真实 M2M 队列：`S_FMT -> REQBUFS -> QUERYBUF -> MMAP -> QBUF -> STREAMON -> poll -> DQBUF -> requeue -> STREAMOFF`。
>>>>>>> 57cb4fd39a36343ee19a989d109b951a768d9a52

本阶段分两条线：

<<<<<<< HEAD
```bash
./scripts/run_00_rk_board_reality_check.sh
```

如果输出 `v4l2_m2m_status=NOT_FOUND` 且 `rkmpp_status=AVAILABLE`，说明真实硬解主线应使用 RKMPP（例如 FFmpeg `h264_rkmpp/hevc_rkmpp`），Stage06 用来学习 V4L2 M2M 通用状态机和 debug 方法，不要强行对 rkisp/camera 节点跑 codec ioctl。详细说明见 [docs/05_rk_board_no_m2m_adaptation.md](docs/05_rk_board_no_m2m_adaptation.md)。

RK 板报告必须明确：

```text
environment=RK_BOARD
backend=rkmpp 或 no_v4l2_m2m_codec_node
hardware_proof=yes|no
what_this_proves=...
what_this_does_not_prove=...
```

## 这个 demo 教什么

Stage06 接在 Stage05 FFmpeg 硬解之后，目标是把“FFmpeg 可能走到硬件后端”继续落到 Linux V4L2 M2M codec 的真实工作语言：`OUTPUT/CAPTURE` 双队列、`S_FMT/REQBUFS/QBUF/DQBUF/STREAMON/STREAMOFF`、`poll timeout`、`SOURCE_CHANGE`、`EOS/drain`、以及如何写 driver-facing debug report。
=======
| 线 | 运行环境 | 做什么 | 不做什么 |
| --- | --- | --- | --- |
| VM/vim2m | VM 上的 `/dev/video0` `vim2m` | 真实执行 V4L2 M2M ioctl、mmap、queue loop、fault injection | 不证明 H.264/H.265 硬件解码 |
| RK/RKMPP | RK 板 | 收集 RKMPP/FFmpeg、设备节点、dmesg 和可选硬解命令证据 | 不把 ISP/camera 节点当 codec M2M |
>>>>>>> 57cb4fd39a36343ee19a989d109b951a768d9a52

## 深化点

| Stage03 | Stage06 |
| --- | --- |
| `QUERYCAP` 判断节点能力 | 继续验证 `V4L2_CAP_VIDEO_M2M` 和 `STREAMING` 后进入队列 |
| `ENUM_FMT` 看支持格式 | `TRY_FMT/S_FMT` 观察驱动回填 `bytesperline/sizeimage/fourcc` |
| 理解 buffer 概念 | 真实 `REQBUFS/QUERYBUF/MMAP/munmap/REQBUFS count=0` |
| 了解 streaming | 真实 `QBUF/STREAMON/poll/DQBUF/requeue/STREAMOFF` |
| 能分类设备节点 | 能用 counters、metrics、gate 判断队列是否真的动了 |
| 概念性 debug | timeout、bytesused、source-change 恢复路径都有可运行证据 |

## 文件结构

```text
include/00_stage06_m2m_common.hpp
src/01_vm_vim2m_device_discovery.cpp
src/02_vm_vim2m_format_negotiation.cpp
src/03_vm_vim2m_mmap_lifecycle.cpp
src/04_vm_vim2m_queue_loop.cpp
src/05_vm_vim2m_fault_injection.cpp
src/06_rk_board_rkmpp_hardware_path.cpp
docs/06_post_code_debugging_guide.md
enterprise_project/
```

## 编译

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage06_v4l2_m2m_deep_dive
./build.sh all-with-enterprise
```

## VM/vim2m 运行

```bash
./scripts/run_all_stage06.sh
```

单独运行真实队列 loop：

```bash
./scripts/run_04_vm_vim2m_queue_loop.sh
```

企业诊断服务：

```bash
./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh
./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh
```

代码跑通之后，按调试教程手动练 CPU、耗时、内存、fd/mmap、queue 和 RK 硬件证据定位：

```bash
sed -n '1,260p' docs/06_post_code_debugging_guide.md
```

## RK 板运行

```bash
./scripts/run_06_rk_board_rkmpp_hardware_path.sh
```

有码流样本时：

```bash
INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./scripts/run_06_rk_board_rkmpp_hardware_path.sh
```

企业服务 RK 模式：

```bash
MODE=rk-rkmpp INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

## 当前 VM 验证结果

在本 VM 上，`/dev/video0` 为 `vim2m`，全量 VM 线已验证通过：

```text
01_vm_vim2m_device_discovery        PASS
02_vm_vim2m_format_negotiation      PASS
03_vm_vim2m_mmap_lifecycle          PASS
04_vm_vim2m_queue_loop              PASS
05_vm_vim2m_fault_injection         PASS
07_enterprise_vm_vim2m_real_queue   PASS
```

关键证据：Demo04 和企业服务都会输出真实 `QBUF VIDEO_OUTPUT/CAPTURE`、`STREAMON`、`poll ret=1`、`DQBUF`、`STREAMOFF`。

## 边界

`vim2m` 是 raw-to-raw 虚拟 M2M 节点，可以验证 V4L2 M2M 队列逻辑，但不是 codec decoder。RK 板若没有 V4L2 codec M2M 节点，应走 RKMPP/FFmpeg 或厂商工具验证硬件解码路径。
