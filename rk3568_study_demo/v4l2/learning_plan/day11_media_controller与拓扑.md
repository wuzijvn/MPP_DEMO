# Day11 Media Controller 与拓扑

## 1. 今日目标

1. 理解为什么 SoC 上有很多 `/dev/videoX`。
2. 看懂 `media-ctl -p` 输出中的 entity/pad/link。
3. 建立“拓扑图思维”。

## 2. 动手命令

```bash
media-ctl -d /dev/media0 -p
media-ctl -d /dev/media1 -p
```

## 3. 你要能回答

1. 哪个 entity 对应主图像输出。
2. 哪些节点是 metadata。
3. 链路里数据从哪里进、到哪里出。

## 4. 代码关联

1. 你当前 USB UVC 场景主要用单节点。
2. 后续切到 SoC ISP 时，拓扑理解是必备前置。

## 5. 验收标准

1. 画一张简化拓扑图（手画拍照也可）。
2. 说清你当前业务该用哪个节点采集。

## 6. 今日交付

1. `day11_topology_notes.md`（5~15 行即可）。
