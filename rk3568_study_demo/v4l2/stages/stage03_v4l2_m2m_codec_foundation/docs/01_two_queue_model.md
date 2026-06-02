# V4L2 M2M 双队列模型

## Decoder 方向

1. `OUTPUT` queue：输入压缩码流，例如 H264/HEVC。
2. `CAPTURE` queue：输出解码帧，例如 NV12/YUV。

名字容易误导：`OUTPUT` 是从用户态输出给驱动，`CAPTURE` 是用户态从驱动捕获结果。

## 最小状态机

1. `S_FMT OUTPUT`：告诉驱动输入格式。
2. `S_FMT CAPTURE`：告诉驱动想要的输出格式。
3. `REQBUFS OUTPUT/CAPTURE`：申请两边 buffer 池。
4. `QBUF CAPTURE`：先给驱动空 frame buffer。
5. `QBUF OUTPUT`：再给驱动输入数据。
6. `STREAMON`：启动两条队列。
7. `DQBUF CAPTURE`：拿回完成的 frame buffer。
8. `STREAMOFF`：停止队列并释放资源。

## 驱动影子线

1. `REQBUFS/QUERYBUF` 进入 videobuf2 buffer 管理。
2. `QBUF` 后 buffer 所有权属于驱动。
3. `DQBUF` 返回后 buffer 所有权回到用户态。
4. timeout 通常说明格式、输入数据、队列顺序或驱动状态机有问题。
