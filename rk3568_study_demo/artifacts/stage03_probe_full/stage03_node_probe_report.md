# Stage03 Node Probe Report

## 1. 背景与目标

在进入 V4L2 M2M 双队列状态机（S_FMT/REQBUFS/QBUF/DQBUF）前，先完成节点能力画像。

## 2. 基础统计

- 生成时间: `20260513_210900`
- 扫描节点数: `0`
- `VIDIOC_QUERYCAP` 成功数: `0`
- M2M 候选节点数（启发式）: `0`

## 3. 总览表

| dev | open | querycap | streaming | m2m_sp | m2m_mp | dual_sp | dual_mp | out_sp | cap_sp | out_mp | cap_mp | driver | card |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|

## 4. 节点详情

## 5. 驱动影子线：这一阶段对应的驱动侧知识

1. `open(/dev/videoX)` 对应 video 设备 file operations，失败通常先看权限/节点归属/驱动注册。
2. `VIDIOC_QUERYCAP` 反映驱动声明能力，`effective_caps` 才是最终判断依据。
3. `VIDIOC_ENUM_FMT` 是驱动对 queue type 的格式暴露接口，后续 `S_FMT` 必须受它约束。
4. 双队列（OUTPUT/CAPTURE）只是起点，真正可用性还要经 `REQBUFS/QBUF/STREAMON/DQBUF` 验证。
5. 常见失败证据入口：`dmesg | grep -Ei "v4l2|vpu|codec|video|timeout|iommu|dma"`。

## 6. 下一步动作

1. 选出 1~2 个候选节点进入 Stage03 状态机实操。
2. 按顺序验证 `S_FMT(OUTPUT/CAPTURE)`、`REQBUFS`、`STREAMON`、`QBUF/DQBUF`。
3. 记录 `STREAMON` 与 `DQBUF` 失败码及 dmesg，形成调试报告。
