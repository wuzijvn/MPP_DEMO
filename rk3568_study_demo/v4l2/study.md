# V4L2 真正能落地的学习手册（零基础版）
使用$soc-video-codec-stack-learning-coach。
> 文档定位：
> 这不是“百科”，是“训练手册”。
> 你看完后要能独立回答：
> 1) V4L2 初始化到底做了什么；
> 2) 为什么必须 QBUF/DQBUF；
> 3) 出错时第一步怎么查；
> 4) 下一步该练什么。

---

## 0. 先说结论（给现在的你）

你现在最该掌握的不是“所有 API”，而是一条稳定主线：

1. 只学采集（Video Capture）
2. 只学单平面（single-planar）
3. 只学 MMAP streaming
4. 只学一套状态机流程

你先把这一套吃透，再学：

1. 控制项（曝光、增益）
2. 多线程
3. 多平面
4. DMABUF
5. 编解码 m2m

如果你跳着学，会很快乱掉。

---

## 1. V4L2 到底是什么（不绕弯）

V4L2 = Linux 视频设备统一接口。

你可以把它看成：

1. 硬件五花八门（USB 摄像头、ISP、编码器）
2. 驱动把硬件差异屏蔽
3. 用户态统一通过 `/dev/videoX + ioctl` 操作

所以 V4L2 本质是“协议”，不是某个库函数集合。

---

## 2. 你必须先理解的三件事

### 2.1 格式协商（Format Negotiation）

你和驱动要先谈妥：

1. 分辨率（width/height）
2. 像素格式（fourcc）
3. 行跨度（bytesperline）
4. 帧大小（sizeimage）

### 2.2 缓冲队列（Buffer Queue）

V4L2 streaming 的核心动作：

1. QBUF：把空缓冲交给驱动
2. DQBUF：从驱动拿回已填帧缓冲
3. QBUF：处理完再还回去

### 2.3 流状态（Streaming State）

1. STREAMON 前：不会持续产帧
2. STREAMON 后：驱动持续写队列
3. STREAMOFF：停止产帧

---

## 3. 初始化到底是啥（你问的重点）

下面是你要背熟的“采集初始化五阶段”：

### 阶段 A：设备与能力

1. open 设备节点
2. VIDIOC_QUERYCAP

目的：确认“这个节点能不能采集、能不能 streaming”。

### 阶段 B：格式与帧率

1. ENUM_FMT / ENUM_FRAMESIZES / ENUM_FRAMEINTERVALS（推荐）
2. TRY_FMT（推荐）
3. S_FMT（必须）
4. G_FMT（推荐，读回真实生效）
5. S_PARM / G_PARM（可选）

目的：把参数协商到“设备真的支持”的组合。

### 阶段 C：缓冲初始化（MMAP）

1. REQBUFS
2. QUERYBUF
3. mmap
4. QBUF（全部先入队）

目的：把数据通道准备好。

### 阶段 D：开流并循环取帧

1. STREAMON
2. select/poll 等待
3. DQBUF 取帧
4. 处理帧
5. QBUF 回队

### 阶段 E：停流和释放

1. STREAMOFF
2. munmap
3. close

---

## 4. 标准状态机（你一定要会口述）

```text
[OPEN]
   |
   v
[QUERYCAP] ---> fail => [EXIT]
   |
   v
[SET_FMT/PARM]
   |
   v
[REQBUFS + MMAP + QBUF all]
   |
   v
[STREAMON]
   |
   v
[WAIT(select/poll)] -> [DQBUF] -> [PROCESS] -> [QBUF] --loop--
   |
   v
[STREAMOFF]
   |
   v
[UNMAP + CLOSE]
```

你以后写的所有采集程序都在这个状态机里。

---

## 5. 这几个结构体先背（其余先不背）

### 5.1 `v4l2_capability`

用途：查询能力（能不能 capture / streaming）

你先关心字段：

1. `capabilities`
2. `device_caps`
3. `driver`
4. `card`

### 5.2 `v4l2_format`

用途：设置/读取格式

你先关心字段：

1. `type`
2. `fmt.pix.width`
3. `fmt.pix.height`
4. `fmt.pix.pixelformat`
5. `fmt.pix.bytesperline`
6. `fmt.pix.sizeimage`

### 5.3 `v4l2_requestbuffers`

用途：申请缓冲池

你先关心字段：

1. `count`
2. `type`
3. `memory`

### 5.4 `v4l2_buffer`

用途：QBUF/DQBUF 的核心容器

你先关心字段：

1. `index`
2. `type`
3. `memory`
4. `bytesused`
5. `length`
6. `m.offset`（MMAP）
7. `timestamp`
8. `sequence`

### 5.5 `v4l2_streamparm`

用途：帧率参数

你先关心字段：

1. `parm.capture.timeperframe`

---

## 6. 初始化相关 ioctl 速查（含前置条件）

### 6.1 `VIDIOC_QUERYCAP`

1. 前置：open 成功
2. 作用：查询能力
3. 常见错误：EINVAL（不是 V4L2 设备）

### 6.2 `VIDIOC_S_FMT`

1. 前置：QUERYCAP 通过
2. 作用：请求格式
3. 注意：驱动可能改你参数
4. 动作：立刻 `G_FMT` 读回

### 6.3 `VIDIOC_REQBUFS`

1. 前置：S_FMT 完成
2. 作用：申请队列缓冲
3. 常见错误：ENOMEM

### 6.4 `VIDIOC_QUERYBUF`

1. 前置：REQBUFS 完成
2. 作用：拿每个缓冲 offset/length
3. 后续：把 offset 喂给 mmap

### 6.5 `VIDIOC_QBUF`

1. 前置：QUERYBUF+mmap 完成
2. 作用：把空缓冲入队给驱动填数据

### 6.6 `VIDIOC_STREAMON`

1. 前置：至少有缓冲已 QBUF
2. 作用：启动采集流

### 6.7 `VIDIOC_DQBUF`

1. 前置：STREAMON 后
2. 作用：取回已填帧缓冲
3. 常见错误：EAGAIN（非阻塞没数据）

### 6.8 `VIDIOC_STREAMOFF`

1. 前置：STREAMON 后
2. 作用：停流并结束循环

---

## 7. 单平面、多平面、read、mmap、dmabuf 到底啥关系

### 7.1 单平面 vs 多平面

1. 单平面：一帧一个数据面（UVC 常见）
2. 多平面：一帧多个 plane（SoC 常见）

### 7.2 I/O 模式

1. read：最简单，性能一般
2. MMAP：主流，最适合你当前阶段
3. USERPTR：复杂，不建议入门就上
4. DMABUF：零拷贝关键，后期重点

你的当前学习主线：

1. 单平面 + MMAP

---

## 8. 像素格式基础（YUYV/MJPG/NV12）

### 8.1 先记 3 个格式

1. YUYV：未压缩，后处理方便，带宽大
2. MJPG：压缩，带宽小，需解码
3. NV12：SoC 常见格式，适合硬件链路

### 8.2 帧大小估算

1. YUYV: `W*H*2`
2. RGB24: `W*H*3`
3. NV12: `W*H*1.5`

例子：640x480 YUYV

1. `640*480*2 = 614400 bytes`

---

## 9. 初学者最容易踩的 10 个坑

1. 选错节点（metadata 当 capture）
2. 没 QUERYCAP 就开始 S_FMT
3. 没 QBUF 就 STREAMON
4. 没 STREAMON 就 DQBUF
5. DQBUF 后不 QBUF 回队
6. 忽略驱动改格式（没读回 G_FMT）
7. 以为 S_PARM 一定生效
8. 忽略 bytesused，直接按理论长度读
9. 忘记处理 EINTR/EAGAIN
10. 只看“能跑”，不做 fps/超时统计

---

## 10. 错误码怎么查（实战版）

### EINVAL

1. 格式不支持
2. 分辨率不支持
3. 调用顺序错误
4. type/memory 填错

### ENOMEM

1. 缓冲申请太大
2. 内存不足

### EAGAIN

1. 非阻塞模式暂时无帧

### EIO / ENODEV

1. 设备中断
2. 热插拔问题

固定定位模板：

1. 记录哪一步失败
2. 记录 errno
3. 记录参数
4. 用 v4l2-ctl 复现
5. 缩小变量 A/B

---

## 11. 代码优先（命令行仅用于验证）

你在公司里应该优先做这件事：先把 API 层写出来，再用命令行做交叉验证。

先写函数，再跑业务：

```cpp
int v4l2_open_device(Ctx* c, const char* dev);
int v4l2_query_caps(Ctx* c);
int v4l2_set_format(Ctx* c, int w, int h, uint32_t fourcc);
int v4l2_reqbufs_mmap(Ctx* c, int count);
int v4l2_queue_all(Ctx* c);
int v4l2_stream_on(Ctx* c);
int v4l2_capture_one(Ctx* c, Frame* f, int timeout_ms); // 内部 poll + DQBUF
int v4l2_requeue(Ctx* c, int index);
int v4l2_stream_off(Ctx* c);
void v4l2_close(Ctx* c);
```

为什么是这套顺序：

1. 和驱动状态机一一对应，出错位置容易定位
2. 每个函数都可单测，不会把 200 行逻辑挤在 `main`
3. 后续从 UVC 切到 MIPI/ISP，大部分代码可复用

命令行工具只在两种场景使用：

1. 设备可用性验证（确认不是代码 Bug）
2. 参数对照（确认格式和帧率是否被驱动改写）

最小验证命令保留三条就够：

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video10 --list-formats-ext
v4l2-ctl -d /dev/video10 --all
```

---

## 12. 初始化伪代码（你可以直接背）

```cpp
fd = open("/dev/videoX", O_RDWR);

ioctl(fd, VIDIOC_QUERYCAP, ...);

ioctl(fd, VIDIOC_S_FMT, ...);
ioctl(fd, VIDIOC_G_FMT, ...);

ioctl(fd, VIDIOC_REQBUFS, ...);
for each i:
    ioctl(fd, VIDIOC_QUERYBUF, ...);
    mmap(...);
for each i:
    ioctl(fd, VIDIOC_QBUF, ...);

ioctl(fd, VIDIOC_STREAMON, ...);
while (running):
    select/poll(...);
    ioctl(fd, VIDIOC_DQBUF, ...);
    process(...);
    ioctl(fd, VIDIOC_QBUF, ...);

ioctl(fd, VIDIOC_STREAMOFF, ...);
munmap(...);
close(fd);
```

---

## 13. 官方文档怎么读才不崩溃（重点）

你不要顺读全站。按这顺序：

### 第一轮（流程）

1. v4l2 总入口
2. common
3. querycap
4. io
5. mmap
6. buffer

### 第二轮（字段）

1. S_FMT / G_FMT / TRY_FMT
2. REQBUFS / QUERYBUF / QBUF / DQBUF
3. STREAMON/OFF
4. control

### 第三轮（进阶）

1. media controller
2. mplane
3. dmabuf
4. request api

每读一页只写三行：

1. 前置条件
2. 必填字段
3. 常见错误码

---

## 14. 给你今天就能执行的学习动作

1. 背“初始化五阶段”。
2. 口述一次状态机流程。
3. 跑一次命令行抓图。
4. 跑一次你自己的采集程序。
5. 写 5 行复盘。

可直接执行：

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2
./build.sh v4l2_capture_one_frame
./bin/v4l2_capture_one_frame /dev/video10 640 480 ../artifacts/today.yuyv ../artifacts/today.ppm
```

---

## 15. 14 天按天计划入口（执行版）

完整计划在：

1. [learning_plan/00_总览_14天路线.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_总览_14天路线.md)
2. [learning_plan/day01_环境与节点识别.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day01_环境与节点识别.md)
3. [learning_plan/day02_格式与帧率协商.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day02_格式与帧率协商.md)
4. [learning_plan/day03_缓冲队列与MMAP.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day03_缓冲队列与MMAP.md)
5. [learning_plan/day04_单帧采集到可视化.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day04_单帧采集到可视化.md)
6. [learning_plan/day05_连续采集与FPS.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day05_连续采集与FPS.md)
7. [learning_plan/day06_参数控制与图像稳定.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day06_参数控制与图像稳定.md)
8. [learning_plan/day07_错误注入与恢复.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day07_错误注入与恢复.md)
9. [learning_plan/day08_像素格式与色彩转换.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day08_像素格式与色彩转换.md)
10. [learning_plan/day09_多线程采集与写盘.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day09_多线程采集与写盘.md)
11. [learning_plan/day10_性能指标与回归基线.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day10_性能指标与回归基线.md)
12. [learning_plan/day11_media_controller与拓扑.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day11_media_controller与拓扑.md)
13. [learning_plan/day12_dmabuf入门与零拷贝思维.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day12_dmabuf入门与零拷贝思维.md)
14. [learning_plan/day13_综合实战任务.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day13_综合实战任务.md)
15. [learning_plan/day14_复盘答辩与面试题.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/day14_复盘答辩与面试题.md)
16. [learning_plan/99_每日打卡模板.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/99_每日打卡模板.md)

补充：如果你希望按“岗位能力”来学，而不是只做 14 天入门，优先看这份重排路线：

1. [learning_plan/00_路线重排_从零到岗位能力.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/learning_plan/00_路线重排_从零到岗位能力.md)

---

## 16. 你后续如果还觉得“看不懂”，就按这个问我

每次只问一个点：

1. 这一步前置条件是什么？
2. 我必须填哪些字段？
3. 驱动回填哪些字段？
4. 失败最可能是啥？

我就按这四条给你讲，不再讲空话。

---

## 17. 博客精读拆解入口（你刚提到的 CSDN 风格文章）

如果你看“全链路讲解文章”还是晕，先看这份拆解版：

1. [博客精读_CSDN_v4l2拆解.md](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/博客精读_CSDN_v4l2拆解.md)

这份文档专门解决：

1. 为什么 `open` 调用图看起来和你 gdb 不一致。
2. `video_device/file_operations/v4l2_file_operations` 的层次关系。
3. `VIDIOC_QUERYCTRL/G_CTRL/S_CTRL` 什么时候用、为什么这样用。
4. 该先学什么、该暂时跳过什么。
