# Stage03 V4L2 M2M Preflight Report

## 1. 背景与目标

本阶段用于识别和筛选 V4L2 M2M 候选节点，作为后续状态机实验的输入。

## 2. 基础统计

- 生成时间: `20260514_164709`
- 扫描节点数: `1`
- `VIDIOC_QUERYCAP` 成功数: `1`
- M2M 候选节点数（启发式）: `1`

- codec-like 节点数（OUTPUT 含压缩格式）: `0`
- 虚拟测试驱动节点数: `1`

## 3. 总览表

| dev | open | querycap | streaming | m2m_sp | m2m_mp | dual_sp | dual_mp | codec_like | out_has_compressed | cap_has_compressed | virtual_test | class | out_sp | cap_sp | out_mp | cap_mp | driver | card |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|---|
| /dev/video0 | 1 | 1 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | virtual-test-m2m | 4 | 9 | 0 | 0 | vim2m | vim2m |

## 4. 节点详情

### 4.1 /dev/video0

- open_ok: `1`
- querycap_ok: `1`
- driver/card/bus: `vim2m` / `vim2m` / `platform:vim2m`
- capabilities: `0x84208000`
- device_caps: `0x04208000`
- effective_caps: `0x04208000`
- heuristic_m2m_candidate: `1`

- heuristic_codec_like: `0`
- heuristic_output_has_compressed: `0`
- heuristic_capture_has_compressed: `0`
- heuristic_virtual_test_driver: `1`
- probe_classification: `virtual-test-m2m`

### OUTPUT single-planar formats

| idx | type | fourcc | pixelformat(hex) | description | flags(hex) |
|---:|---|---|---|---|---|
| 0 | VIDEO_OUTPUT | RGBP | 0x50424752 | 16-bit RGB 5-6-5 | 0x00000000 |
| 1 | VIDEO_OUTPUT | RGBR | 0x52424752 | 16-bit RGB 5-6-5 BE | 0x00000000 |
| 2 | VIDEO_OUTPUT | RGB3 | 0x33424752 | 24-bit RGB 8-8-8 | 0x00000000 |
| 3 | VIDEO_OUTPUT | BGR3 | 0x33524742 | 24-bit BGR 8-8-8 | 0x00000000 |

### CAPTURE single-planar formats

| idx | type | fourcc | pixelformat(hex) | description | flags(hex) |
|---:|---|---|---|---|---|
| 0 | VIDEO_CAPTURE | RGBP | 0x50424752 | 16-bit RGB 5-6-5 | 0x00000000 |
| 1 | VIDEO_CAPTURE | RGBR | 0x52424752 | 16-bit RGB 5-6-5 BE | 0x00000000 |
| 2 | VIDEO_CAPTURE | RGB3 | 0x33424752 | 24-bit RGB 8-8-8 | 0x00000000 |
| 3 | VIDEO_CAPTURE | BGR3 | 0x33524742 | 24-bit BGR 8-8-8 | 0x00000000 |
| 4 | VIDEO_CAPTURE | YUYV | 0x56595559 | YUYV 4:2:2 | 0x00000000 |
| 5 | VIDEO_CAPTURE | BA81 | 0x31384142 | 8-bit Bayer BGBG/GRGR | 0x00000000 |
| 6 | VIDEO_CAPTURE | GBRG | 0x47524247 | 8-bit Bayer GBGB/RGRG | 0x00000000 |
| 7 | VIDEO_CAPTURE | GRBG | 0x47425247 | 8-bit Bayer GRGR/BGBG | 0x00000000 |
| 8 | VIDEO_CAPTURE | RGGB | 0x42474752 | 8-bit Bayer RGRG/GBGB | 0x00000000 |

### OUTPUT multi-planar formats

(none)

### CAPTURE multi-planar formats

(none)

提示：该节点属于常见虚拟测试驱动（如 vim2m/vicodec/vivid）。
适合学习 ioctl 状态机，不可直接作为真实 SoC H264/H265 硬件编解码能力证明。

## 5. 驱动影子线：这一阶段对应的驱动侧知识

1. `open(/dev/videoX)` 进入 video 设备 file operations。
2. `VIDIOC_QUERYCAP` 对应驱动能力声明；若有 `DEVICE_CAPS`，以它为准。
3. `VIDIOC_ENUM_FMT` 暴露可协商格式集合；S_FMT 受它约束。
4. 候选只是第一步，是否可用要看完整 QBUF/DQBUF 状态机。

## 6. 下一步动作

1. 从候选节点里选 1~2 个进入 m2m-sequence。
2. 验证 `S_FMT/REQBUFS/QUERYBUF/QBUF/STREAMON/DQBUF`。
3. 记录失败步骤、errno、dmesg。
