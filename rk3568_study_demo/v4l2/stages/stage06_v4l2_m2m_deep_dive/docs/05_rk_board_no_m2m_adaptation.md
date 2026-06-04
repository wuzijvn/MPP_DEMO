# RK Board Adaptation

## 结论

RK 板端不要假设存在可用的 V4L2 M2M codec 节点。Stage06 现在提供两份可运行代码：

<<<<<<< HEAD
这条结论现在是整个 codec 学习计划的正式边界：VM 负责 V4L2 M2M 逻辑，RK 板负责 RKMPP 真实硬件证据。总路线见 `../../../learning_plan/00_dual_environment_codec_route.md`。

## 当前板端证据
=======
| 版本 | 文件 | 用途 |
| --- | --- | --- |
| VM/vim2m | `src/01` 到 `src/05`、企业 `--mode=vm-vim2m` | 用虚拟 M2M 节点真实验证 V4L2 M2M 队列逻辑 |
| RK/RKMPP | `src/06_rk_board_rkmpp_hardware_path.cpp`、企业 `--mode=rk-rkmpp` | 用 RKMPP/FFmpeg/板端日志验证硬件路径 |
>>>>>>> 57cb4fd39a36343ee19a989d109b951a768d9a52

## RK 板命令

```bash
./scripts/run_06_rk_board_rkmpp_hardware_path.sh
```

带样本：

```bash
INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./scripts/run_06_rk_board_rkmpp_hardware_path.sh
```

企业模式：

```bash
MODE=rk-rkmpp INPUT=/path/to/sample.h264 DECODER=h264_rkmpp \
  ./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

## RK 证据文件

RK 脚本会生成：

```text
uname.txt
device_nodes.txt
v4l2_list_devices.txt
ffmpeg_decoders.txt
dmesg_media_hints.txt
ffmpeg_rkmpp_decode.log
rk_rkmpp_report.md
```

如果 `h264_rkmpp/hevc_rkmpp` 不存在，默认只说明“证据已采集”；如果你要求硬件解码器必须存在，则加 `--require-rkmpp` 或企业 `EXTRA_ARGS="--require-rkmpp"`。

## 不要做

1. 不要把 `/dev/video0` 默认当 codec decoder。
2. 不要把 `vim2m` 的 `DQBUF` 当 RK VPU 硬解证据。
3. 不要发明 RKMPP SDK 函数或私有 ABI。
4. 不要删除 RK 硬件路径，只因为 VM 可以用 `vim2m` 跑通。

<<<<<<< HEAD
## 双环境验收写法

VM 报告写法：

```text
environment=VM
backend=v4l2_m2m_virtual
hardware_proof=no
what_this_proves=V4L2 M2M ioctl sequence and queue ownership are understood
what_this_does_not_prove=RK3568 VPU hardware decode performance or RKMPP availability
```

RK 板报告写法：

```text
environment=RK_BOARD
backend=rkmpp
hardware_proof=yes
what_this_proves=FFmpeg can use h264_rkmpp/hevc_rkmpp path on this board
what_this_does_not_prove=A generic V4L2 M2M codec node exists on this board
```

## 面试/入职表达模板
=======
## 面试/入职表达
>>>>>>> 57cb4fd39a36343ee19a989d109b951a768d9a52

我会把 V4L2 M2M 学习和 RK 板真实硬解验证分开：VM 上用 `vim2m` 真实跑 M2M queue 逻辑，证明 `REQBUFS/MMAP/QBUF/DQBUF/poll` 状态机；RK 板上用 FFmpeg `h264_rkmpp/hevc_rkmpp` 或厂商工具收集硬件路径证据，不把 camera/ISP 节点误判成 codec M2M。
