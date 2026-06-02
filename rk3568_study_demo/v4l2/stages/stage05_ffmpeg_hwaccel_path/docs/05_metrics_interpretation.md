# Stage05 指标解释

## 关键指标

1. `frame_hw`：被识别为硬件像素格式的帧数。
2. `frame_sw`：软件帧数。
3. `fallback_count`：发生回退的次数（设备创建失败、无 hwfmt、强制注入）。
4. `hw_transfer_ok/fail`：`av_hwframe_transfer_data` 成败。

## 趋势判断

1. 正常硬件路径：`frame_hw > 0`，`fallback_count` 低。
2. 软回退路径：`frame_hw = 0`，`frame_sw` 持续增长。
3. 回拷失败：`hw_transfer_fail` 持续增加，需查后端和格式。

## 常见误判

1. 命令写了 `-hwaccel` 就认为硬解成功（错误）。
2. 只看 fps 不看帧类型（错误）。
3. 忽略 dmesg 和设备节点（错误）。
