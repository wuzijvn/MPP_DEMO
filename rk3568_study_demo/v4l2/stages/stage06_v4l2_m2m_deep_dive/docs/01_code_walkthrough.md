# Stage06 Code Walkthrough

## 主流程

```text
open
  -> VIDIOC_QUERYCAP
  -> VIDIOC_ENUM_FMT
  -> VIDIOC_S_FMT OUTPUT/CAPTURE
  -> VIDIOC_REQBUFS
  -> VIDIOC_QUERYBUF
  -> mmap
  -> VIDIOC_QBUF OUTPUT/CAPTURE
  -> VIDIOC_STREAMON
  -> poll
  -> VIDIOC_DQBUF OUTPUT/CAPTURE
  -> requeue
  -> VIDIOC_STREAMOFF
  -> munmap
  -> VIDIOC_REQBUFS count=0
  -> close
```

## 关键文件

| 文件 | 代码重点 | 驱动影子线 |
| --- | --- | --- |
| `00_stage06_m2m_common.hpp` | `xioctl`、format、buffer、poll helper | ioctl ops、vb2 queue、waitqueue |
| `01_vm_vim2m_device_discovery.cpp` | `QUERYCAP/ENUM_FMT` | 设备节点分类 |
| `02_vm_vim2m_format_negotiation.cpp` | `TRY_FMT/S_FMT` | format negotiation |
| `03_vm_vim2m_mmap_lifecycle.cpp` | `REQBUFS/QUERYBUF/MMAP` | vb2 buffer allocation/mapping |
| `04_vm_vim2m_queue_loop.cpp` | `QBUF/DQBUF/poll/requeue` | buffer ownership and completion |
| `05_vm_vim2m_fault_injection.cpp` | `bytesused=0`、unsupported format、poll without streamon | errno、format fallback、timeout |
| `06_rk_board_rkmpp_hardware_path.cpp` | FFmpeg/RKMPP evidence command | vendor stack/hardware path |
| `enterprise_project/src/06_m2m_diagnostic_service.cpp` | 双模式服务、恢复、gate | bring-up diagnostic service |

## OUTPUT/CAPTURE 所有权

```text
USER fills OUTPUT buffer
  -> QBUF OUTPUT
DRIVER owns OUTPUT buffer
  -> DQBUF OUTPUT
USER owns OUTPUT buffer again

USER gives empty CAPTURE buffer
  -> QBUF CAPTURE
DRIVER owns CAPTURE buffer
  -> poll wakes
  -> DQBUF CAPTURE
USER owns completed CAPTURE buffer again
```

在 `vim2m` 中，OUTPUT/CAPTURE 都是 raw frame buffer。真实 codec decoder 中，OUTPUT 通常是 compressed bitstream packet，CAPTURE 是 decoded raw frame。

## 故障路径

| 故障 | VM/vim2m 代码必须做什么 | 观察 |
| --- | --- | --- |
| `bytesused_zero` | 真实 `VIDIOC_QBUF OUTPUT bytesused=0` | driver 接受或拒绝都记录 verdict |
| `unsupported_format` | 真实 `VIDIOC_S_FMT H264` | 失败或回填成 RGBP 都算捕获 |
| `timeout` | 真实 `poll(0)` 和恢复 `STREAMOFF/QBUF/STREAMON` | `PASS_WITH_RECOVERY_EVIDENCE` |
| `source_change` | 真实 CAPTURE `STREAMOFF/REQBUFS 0/S_FMT/REQBUFS/QBUF/STREAMON` | 训练重配顺序 |

## Stage03 到 Stage06 的差异

Stage03 看到 capability 和格式列表就能结束；Stage06 必须能证明 buffer 进入驱动、驱动完成 buffer、用户态拿回 buffer，并且异常时资源释放顺序正确。
