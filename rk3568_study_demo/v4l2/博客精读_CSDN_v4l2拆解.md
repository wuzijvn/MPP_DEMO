# 博客精读（API工程版）：V4L2 不靠命令行，靠函数封装落地

> 你这条反馈很对：公司开发主要是写 C/C++ 调用 V4L2 API，不是天天敲 `v4l2-ctl`。
> 这份文档改成“API开发视角”：
> 1) 讲函数怎么封装；
> 2) 讲结构体字段怎么填；
> 3) 讲错误怎么处理；
> 4) 讲代码如何组织成可维护模块。

---

## 0. 你先记住一句话

V4L2 用户态开发本质：

1. `open`
2. 一组 `ioctl(VIDIOC_*)`
3. `mmap/select/poll`
4. 循环 `DQBUF/QBUF`

命令行工具只是“验证器”，不是“产品代码形态”。

---

## 1. 公司里常见的 V4L2 代码组织（推荐）

别把所有逻辑写在 `main()`，建议拆成模块。

### 1.1 目录结构示例

```text
v4l2_capture/
  include/
    v4l2_capture.h
  src/
    v4l2_device.cpp      // open/close/querycap
    v4l2_format.cpp      // enum/try/set format
    v4l2_buffer.cpp      // reqbufs/querybuf/mmap/qbuf/dqbuf
    v4l2_stream.cpp      // streamon/streamoff/wait/dequeue/requeue
    v4l2_control.cpp     // queryctrl/g_ctrl/s_ctrl/ext_ctrl
    v4l2_error.cpp       // errno->日志映射
  app/
    capture_main.cpp
```

### 1.2 为什么这么拆

1. 初始化逻辑可复用
2. 错误处理集中化
3. 单元测试更容易
4. 后续切换设备（UVC -> ISP）改动可控

---

## 2. API状态机（工程里必须有）

```text
INIT_NONE
  -> DEVICE_OPENED
  -> CAPS_READY
  -> FORMAT_READY
  -> BUFFERS_MAPPED
  -> STREAMING
  -> STOPPED
  -> CLOSED
```

你每个 API 都要定义“前置状态”和“后置状态”。

例：

1. `v4l2_stream_on()` 只能在 `BUFFERS_MAPPED` 后调用。
2. `v4l2_dequeue()` 只能在 `STREAMING` 状态调用。

---

## 3. 你真正需要的 API 封装清单

下面这一组函数，就是你在公司里会写的“基础 V4L2 SDK 层”。

```cpp
int  v4l2_open_device(V4L2Ctx* ctx, const char* dev);
int  v4l2_query_caps(V4L2Ctx* ctx);
int  v4l2_enum_formats(V4L2Ctx* ctx, std::vector<FormatInfo>* out);
int  v4l2_try_format(V4L2Ctx* ctx, int w, int h, uint32_t pixfmt, V4L2FormatResult* out);
int  v4l2_set_format(V4L2Ctx* ctx, int w, int h, uint32_t pixfmt, V4L2FormatResult* out);
int  v4l2_set_fps(V4L2Ctx* ctx, int num, int den);
int  v4l2_reqbufs_mmap(V4L2Ctx* ctx, int count);
int  v4l2_queue_all(V4L2Ctx* ctx);
int  v4l2_stream_on(V4L2Ctx* ctx);
int  v4l2_wait_frame(V4L2Ctx* ctx, int timeout_ms);
int  v4l2_dequeue(V4L2Ctx* ctx, FrameView* frame);
int  v4l2_requeue(V4L2Ctx* ctx, int index);
int  v4l2_stream_off(V4L2Ctx* ctx);
void v4l2_close_device(V4L2Ctx* ctx);

int  v4l2_ctrl_query(V4L2Ctx* ctx, uint32_t id, v4l2_queryctrl* q);
int  v4l2_ctrl_get(V4L2Ctx* ctx, uint32_t id, int* value);
int  v4l2_ctrl_set(V4L2Ctx* ctx, uint32_t id, int value);
```

---

## 4. 每个 API 该填什么字段（核心）

下面给你按调用顺序讲“必须字段”。

---

### 4.1 `v4l2_open_device`

内部做：

1. `ctx->fd = open(dev, O_RDWR | O_CLOEXEC);`

注意：

1. 不建议写死 `/dev/video0`。
2. 设备节点应来自配置或探测结果。

---

### 4.2 `v4l2_query_caps` -> `VIDIOC_QUERYCAP`

结构体：`v4l2_capability cap`。

必须检查：

1. `cap.capabilities & V4L2_CAP_VIDEO_CAPTURE`（或 MPLANE 版本）
2. `cap.capabilities & V4L2_CAP_STREAMING`

不满足直接失败，不要硬跑后续流程。

---

### 4.3 `v4l2_set_format` -> `VIDIOC_S_FMT`

结构体：`v4l2_format fmt`。

必须填：

1. `fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE`
2. `fmt.fmt.pix.width`
3. `fmt.fmt.pix.height`
4. `fmt.fmt.pix.pixelformat`
5. `fmt.fmt.pix.field = V4L2_FIELD_NONE`

必须读回：

1. `fmt.fmt.pix.width/height`（可能被改）
2. `fmt.fmt.pix.bytesperline`
3. `fmt.fmt.pix.sizeimage`

---

### 4.4 `v4l2_set_fps` -> `VIDIOC_S_PARM`（可选）

结构体：`v4l2_streamparm parm`。

填法：

1. `parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE`
2. `parm.parm.capture.timeperframe = {num, den}`

常见误区：

1. 以为设置一定生效。实际上很多 UVC 会近似或忽略。

---

### 4.5 `v4l2_reqbufs_mmap` -> `VIDIOC_REQBUFS`

结构体：`v4l2_requestbuffers req`。

必须填：

1. `req.count`
2. `req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE`
3. `req.memory = V4L2_MEMORY_MMAP`

后续动作：

1. `QUERYBUF` 获取每个 `index` 的 offset/length
2. `mmap` 建立用户态映射

---

### 4.6 `v4l2_queue_all` -> `VIDIOC_QBUF`

结构体：`v4l2_buffer buf`。

必须填：

1. `buf.type`
2. `buf.memory`
3. `buf.index`

必须先把所有空缓冲入队，再 `STREAMON`。

---

### 4.7 `v4l2_stream_on` -> `VIDIOC_STREAMON`

结构体：`v4l2_buf_type type`。

必须填：

1. `type = V4L2_BUF_TYPE_VIDEO_CAPTURE`

---

### 4.8 `v4l2_wait_frame` -> `select/poll`

推荐：

1. 用 `poll` 或 `select`，不要 busy-loop。
2. 超时要计数并上报。

---

### 4.9 `v4l2_dequeue` -> `VIDIOC_DQBUF`

结构体：`v4l2_buffer buf`。

必须填：

1. `buf.type`
2. `buf.memory`

取回后要读：

1. `buf.index`
2. `buf.bytesused`
3. `buf.timestamp`
4. `buf.sequence`

---

### 4.10 `v4l2_requeue` -> `VIDIOC_QBUF`

你处理完数据必须回队，不然缓冲池会耗尽。

---

### 4.11 `v4l2_stream_off` -> `VIDIOC_STREAMOFF`

停流后再 `munmap`，最后 `close`。

---

## 5. 关键数据结构建议（公司代码风格）

```cpp
struct MMapBuffer {
    void*  start;
    size_t length;
};

struct FrameView {
    int      index;
    void*    data;
    uint32_t bytes_used;
    uint64_t timestamp_us;
    uint32_t sequence;
};

struct V4L2Ctx {
    int fd;
    std::string dev;
    int width;
    int height;
    uint32_t pixfmt;
    uint32_t bytesperline;
    uint32_t sizeimage;
    std::vector<MMapBuffer> bufs;
    bool streaming;
};
```

这个上下文对象比“全局变量散落”更可维护。

---

## 6. 错误处理：别只 `perror`，要结构化

### 6.1 包装 ioctl（处理 EINTR）

```cpp
static int xioctl(int fd, unsigned long req, void* arg) {
    int ret;
    do {
        ret = ioctl(fd, req, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}
```

### 6.2 统一错误返回

建议返回统一错误码并保留 `errno`：

1. `-EINVAL` 参数/顺序问题
2. `-EIO` 设备异常
3. `-ETIMEDOUT` 等待超时

### 6.3 日志必须带上下文

至少打印：

1. 设备节点
2. ioctl 名称
3. errno
4. 当前状态

---

## 7. 控制项 API（你笔记里那段）工程化写法

你写的三步法非常对，但公司里会封成函数：

```cpp
int v4l2_ctrl_set(V4L2Ctx* ctx, uint32_t id, int value) {
    v4l2_queryctrl q{};
    q.id = id;
    if (xioctl(ctx->fd, VIDIOC_QUERYCTRL, &q) < 0) return -errno;
    if (q.flags & V4L2_CTRL_FLAG_DISABLED) return -ENOTSUP;
    if (value < q.minimum || value > q.maximum) return -ERANGE;

    v4l2_control c{};
    c.id = id;
    c.value = value;
    if (xioctl(ctx->fd, VIDIOC_S_CTRL, &c) < 0) return -errno;
    return 0;
}
```

### 7.1 什么时候用 `v4l2_control`

1. 单个整型控制项（亮度/对比度等）

### 7.2 什么时候用 `v4l2_ext_controls`

1. 批量控制
2. 复杂控制项（菜单、字符串、64bit等）

### 7.3 常见坑

1. 自动曝光开着，手动曝光值写了也没效果。
2. 先 `QUERYCTRL` 再 set，别盲设。

---

## 8. 单平面与多平面 API 差异（你后续一定会遇到）

### 8.1 单平面

1. `type = V4L2_BUF_TYPE_VIDEO_CAPTURE`
2. `fmt.pix.*`
3. `v4l2_buffer` 不需要 planes 数组

### 8.2 多平面

1. `type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE`
2. `fmt.pix_mp.*`
3. `v4l2_buffer` 里要带 `planes` 指针和数量

公司项目里 SoC 节点很可能是多平面，UVC 常见单平面。

---

## 9. 为什么你看博客里的 `open` 图，gdb 对不上

原因不是你操作错，而是层次不同：

1. 你用 gdb 主要看到用户态 + syscall 边界。
2. 图里后半段是内核态（`v4l2_open/uvc_v4l2_open`）。
3. 内核态通常用 ftrace/perf/tracepoint 看，不是普通用户态 gdb。

所以“看不到图里全部节点”是正常现象。

---

## 10. API级最小主循环模板（公司风格）

```cpp
V4L2Ctx ctx{};
CHECK(v4l2_open_device(&ctx, "/dev/video10"));
CHECK(v4l2_query_caps(&ctx));
CHECK(v4l2_set_format(&ctx, 640, 480, V4L2_PIX_FMT_YUYV, nullptr));
CHECK(v4l2_reqbufs_mmap(&ctx, 4));
CHECK(v4l2_queue_all(&ctx));
CHECK(v4l2_stream_on(&ctx));

for (;;) {
    if (v4l2_wait_frame(&ctx, 2000) < 0) {
        // timeout or interrupted
        continue;
    }

    FrameView f{};
    CHECK(v4l2_dequeue(&ctx, &f));

    // 业务处理：编码、算法、写盘、网络发送...
    process_frame(f.data, f.bytes_used, f.sequence, f.timestamp_us);

    CHECK(v4l2_requeue(&ctx, f.index));
}

v4l2_stream_off(&ctx);
v4l2_close_device(&ctx);
```

---

## 11. 你今天该做的“API化”任务（不是命令行）

### 任务 1

把当前 `v4l2_capture_one_frame.cpp` 里的流程拆成 5 个函数：

1. `open+querycap`
2. `set format`
3. `init mmap buffers`
4. `capture one frame`
5. `cleanup`

### 任务 2

给每个函数加注释：

1. 前置条件
2. 输入参数
3. 输出/副作用
4. 失败时返回

### 任务 3

加一个 `v4l2_ctrl_set_brightness(int value)` 封装（query/get/set 完整链）。

---

## 12. 你后续问我就按这个模板（我会更快帮你）

每次只给我一个 API 名字，比如：

1. `VIDIOC_QUERYBUF`

并按四句问：

1. 前置条件是啥？
2. 哪些字段必须填？
3. 哪些字段是驱动回填？
4. 常见失败和修复？

我会按工程实战方式回答，不再给你“只看命令行”的内容。

---

## 13. 关联入口

1. 总手册：
   - [study.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/study.md)
2. 按天计划：
   - [learning_plan/00_总览_14天路线.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_总览_14天路线.md)
3. 你的笔记：
   - [笔记.txt](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/笔记.txt)
