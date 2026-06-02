# Stage03 驱动影子线：完整路径映射

## 1. open / QUERYCAP

1. 用户态：`open(/dev/videoX)` + `VIDIOC_QUERYCAP`。
2. 驱动侧：file_operations.open + vidioc_querycap。
3. 价值：判定节点可用性和能力位。

## 2. S_FMT（OUTPUT/CAPTURE）

1. 用户态：分别对 OUTPUT/CAPTURE 调 S_FMT。
2. 驱动侧：格式协商回调，可能改写尺寸/stride/plane 配置。
3. 风险：格式不匹配导致 EINVAL。

## 3. REQBUFS/QUERYBUF/MMAP

1. 用户态：申请 buffer 池，查询每个 buffer 的偏移并 mmap。
2. 驱动侧：vb2 分配、队列元数据建立、内存后端对接。
3. 风险：count 过大、memory 模式不兼容、映射失败。

## 4. QBUF/DQBUF

1. 用户态：QBUF 交所有权，DQBUF 取所有权。
2. 驱动侧：队列推进、硬件任务调度、完成中断后归还。
3. 风险：顺序错误会导致 timeout 或队列卡死。

## 5. STREAMON/OFF

1. 用户态：启动/停止双队列状态机。
2. 驱动侧：启停管线、清理 in-flight 资源。
3. 风险：半成功不回滚会导致状态污染。

## 6. SOURCE_CHANGE/EOS/drain

1. 用户态：事件分支、CAPTURE 重配、收敛最后帧。
2. 驱动侧：事件上报、分辨率切换状态机、尾帧回收契约。
3. 风险：漏重配或漏 drain 导致后续异常。

## 7. 当前阶段可推迟内容

1. 具体硬件寄存器编程。
2. 固件命令格式细节。
3. runtime PM/DVFS 深层实现。

本阶段目标是：先把用户态流程与驱动概念对齐，能排障、能沟通、能复盘。
