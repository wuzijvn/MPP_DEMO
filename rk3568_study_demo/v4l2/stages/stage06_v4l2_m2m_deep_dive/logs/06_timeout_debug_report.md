# V4L2 M2M Debug Report - dqbuf_timeout

## 问题现象
- 现象：`poll` 或 `VIDIOC_DQBUF` 超时，CAPTURE 队列没有返回 decoded frame。
- timeout_count：1
- bytesused_zero：0
- source_change_seen：0

## 复现命令
```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/videoX --all
v4l2-ctl -d /dev/videoX --list-formats-ext
dmesg | grep -Ei 'v4l2|m2m|vpu|rkvdec|mpp|codec|timeout|reset|iommu|dma'
```

## 分层定位
| 层级 | 可能原因 | 证据 | 下一步 |
| --- | --- | --- | --- |
| 命令/输入 | 码流不是 Annex B、缺 SPS/PPS、codec 选错 | ffprobe/码流 parser | 先用软件解码验证输入 |
| V4L2 队列 | OUTPUT/CAPTURE QBUF 顺序错、bytesused=0 | qbuf/dqbuf counter | 打印每个 buffer index/bytesused |
| 格式协商 | CAPTURE 格式/stride/sizeimage 未按驱动返回值更新 | TRY_FMT/S_FMT 返回值 | 记录驱动回填格式 |
| source change | 分辨率变化后未重配 CAPTURE | SOURCE_CHANGE event | STREAMOFF CAPTURE 后重新 REQBUFS |
| 驱动/硬件 | IRQ 未完成、firmware timeout、runtime PM | dmesg/trace | 给驱动同学完整日志 |

## 驱动侧可能原因
1. vb2 buffer 状态没有从 active 回到 done，用户态表现为 DQBUF timeout。
2. VPU job 提交后没有 IRQ completion，可能是硬件 hang、firmware 错误或中断未到。
3. runtime PM/autosuspend 让 VPU clock/power 在 job 期间异常关闭。
4. source change 后 CAPTURE queue 生命周期处理不完整。

## 验收结论模板
- 已证明软件输入有效：是/否。
- 已证明 V4L2 格式协商成功：是/否。
- 已证明 OUTPUT bytesused 合理：是/否。
- 已证明 CAPTURE buffer 足够且已 QBUF：是/否。
- 驱动侧假设是否有 dmesg 支撑：是/否。
