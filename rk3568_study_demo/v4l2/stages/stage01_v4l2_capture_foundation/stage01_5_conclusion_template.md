# Stage01.5 结论表模板（直接填写）

- 日期：
- 设备节点：`/dev/video10`
- 摄像头：
- 环境备注（光照/USB占用/系统负载）：

---

## A. 基线（640x480, YUYV, req-bufs=4）

- 命令：
- fps：
- timeout：
- dq_fail/eagain：
- bytesused(min/max/avg)：
- dq host interval(avg)：

结论（1句话）：

---

## B. 队列深度对比（同分辨率同格式）

| req-bufs | fps | timeout | dq_fail | bytesused特征 | 结论 |
|---|---:|---:|---:|---|---|
| 2 |  |  |  |  |  |
| 4 |  |  |  |  |  |
| 8 |  |  |  |  |  |

结论（2句话）：
1.
2.

---

## C. 像素格式对比（同分辨率）

| pixfmt(request) | active(S_FMT) | G_FMT | fps | timeout | bytesused分布 | 结论 |
|---|---|---|---:|---:|---|---|
| YUYV |  |  |  |  |  |  |
| MJPG |  |  |  |  |  |  |

结论（2句话）：
1.
2.

---

## D. 分辨率对比（同格式 YUYV）

| resolution(request) | active(G_FMT) | fps | timeout | dq interval(avg) | 结论 |
|---|---|---:|---:|---:|---|
| 640x480 |  |  |  |  |  |
| 1280x720 |  |  |  |  |  |

结论（2句话）：
1.
2.

---

## E. 故障注入（skip-requeue）

- 命令：
- 触发帧：
- 最终现象：
- `requeue_skipped`：
- `select_timeout`：

你解释的因果链：
1.
2.
3.

---

## 本阶段总复盘（必须填）

### 1) 我现在能讲清楚的 5 个点
1.
2.
3.
4.
5.

### 2) 我还不清楚的 3 个点
1.
2.
3.

### 3) 下一阶段（Stage02）我重点关注
1.
2.
3.
