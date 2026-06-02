# Stage02 驱动渗透 02：vb2 生命周期最小伪代码（教学版）

> 目标：你不需要现在就写内核驱动，但要先看懂典型 `vb2_ops` 生命周期。

## 1. 最小伪代码（不是可编译内核代码）

```c
// 伪代码：展示结构，不是完整驱动

struct my_ctx {
    spinlock_t qlock;
    struct list_head hw_pending;
    bool streaming;
};

static int my_queue_setup(...) {
    // 决定 num_buffers / num_planes / sizes[]
    return 0;
}

static int my_buf_prepare(struct vb2_buffer *vb) {
    // 校验 buffer 大小是否够
    // 做必要的 metadata 初始化
    return 0;
}

static void my_buf_queue(struct vb2_buffer *vb) {
    // 加锁，把 buffer 放入硬件待处理队列
    // 触发硬件任务（若当前空闲）
}

static int my_start_streaming(struct vb2_queue *q, unsigned int count) {
    // 打开时钟/电源
    // 启动 DMA/硬件
    ctx->streaming = true;
    return 0;
}

static void my_stop_streaming(struct vb2_queue *q) {
    // 停止硬件任务
    // 把未完成 buffer 以 ERROR/DONE 方式回收
    ctx->streaming = false;
}

// 中断/下半部（伪代码）
static irqreturn_t my_irq_handler(...) {
    // 读取硬件完成状态
    // 找到对应 vb2_buffer
    // vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
    return IRQ_HANDLED;
}
```

---

## 2. 这段伪代码和你 Stage02 用户态怎么对应

1. 你调用 `REQBUFS` -> 触发 `queue_setup`
2. 你调用 `QBUF` -> 触发 `buf_prepare + buf_queue`
3. 你调用 `STREAMON` -> 触发 `start_streaming`
4. 硬件中断完成 -> `vb2_buffer_done`
5. 你调用 `DQBUF` -> 从 done 队列取 buffer
6. 你调用 `STREAMOFF` -> 触发 `stop_streaming`

---

## 3. 你现在最需要懂的 5 个基础概念

1. **buffer ownership（所有权）**
- QBUF 后 buffer 所有权在驱动
- DQBUF 后所有权回到用户态

2. **不能漏回队**
- 用户态 DQ 后不 Q，会把环路耗尽
- 你在 Stage01 skip-requeue 已验证过

3. **为什么要中断**
- 驱动通常靠中断知道“这个 buffer 处理完成了”
- 没有完成事件，DQ 会卡

4. **为什么要 spinlock**
- 队列在中断上下文和进程上下文都可能访问
- 需要短临界区保护共享状态

5. **为什么不能在持锁区做慢操作**
- 持锁时间过长会放大抖动，甚至死锁风险

---

## 4. 常见故障如何映射

1. 现象：`select timeout` 变多
- 可能：驱动没产生完成事件、回队不足、硬件卡住

2. 现象：`dq_eagain` 偶发
- 可能：非阻塞语义短暂无帧，可重试

3. 现象：`streamon` 失败
- 可能：格式协商和硬件要求不匹配，或资源初始化失败

4. 现象：停止后再次启动异常
- 可能：`stop_streaming` 清理不完整

---

## 本阶段总结：通过这些例子你学到了什么

1. 你看懂了 vb2 的最小生命周期。
2. 你能把 Stage02 用户态行为映射到驱动端动作。
3. 你理解了 spinlock、ownership、done queue 在编解码驱动中的基础作用。
