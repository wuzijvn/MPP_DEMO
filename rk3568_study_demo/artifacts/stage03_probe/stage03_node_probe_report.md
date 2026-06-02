# Stage03 Node Probe Report

- 生成时间: `20260513_202923`
- 扫描节点数: `0`
- `VIDIOC_QUERYCAP` 成功数: `0`
- M2M 候选节点数: `0`

## 结果表

| dev | open | querycap | m2m_sp | m2m_mp | out_sp | out_mp | cap_sp | cap_mp | driver | card |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|

## 驱动影子线：这一阶段对应的驱动侧知识

1. `open(/dev/videoX)` 会进入视频设备的 file operations。
2. `VIDIOC_QUERYCAP` 反映驱动声明的能力位，不代表所有路径都可用。
3. `VIDIOC_ENUM_FMT` 对应驱动对 queue type 的格式枚举能力。
4. 若节点可打开但无 M2M capability，通常不是编解码 M2M 节点。
