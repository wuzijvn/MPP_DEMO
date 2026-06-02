# Stage1 代码精讲（逐模块版）

> 目标：你跟着这份文档 + 代码，能把 V4L2 单节点采集链路讲清楚、改得动、查得出问题。  
> 对象：你现在的阶段（基础薄弱，但要快速进入岗位能力）。

## 1. 先建立心智模型

你现在这套 demo 本质是一个“生产者-消费者”环：

1. 你先把空 buffer 交给驱动（`QBUF`）
2. 驱动采到图后把已填充 buffer 交还给你（`DQBUF`）
3. 你处理完再把该 buffer 还回驱动（`QBUF`）
4. 不断循环

一句话：**不回队就会饿死**。

## 2. 文件分工（先背这个）

1. [`stage01_v4l2_capture_main.cpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture_main.cpp)  
职责：主流程编排（参数 -> 采集 -> 保存）

2. [`stage01_v4l2_args.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_args.hpp)  
职责：默认配置、参数解析、参数校验

3. [`stage01_v4l2_capture.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture.hpp)  
职责：V4L2 关键 ioctl 状态机

4. [`stage01_v4l2_stats.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_stats.hpp)  
职责：统计项累积与 summary 打印

5. [`stage01_v4l2_image.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_image.hpp)  
职责：YUYV -> RGB24 -> PPM

6. [`stage01_v4l2_common.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_common.hpp)  
职责：通用工具（`xioctl`、fourcc、时间）

7. [`stage01_v4l2_types.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_types.hpp)  
职责：结构体定义

## 3. 主入口怎么读

先读 [`stage01_v4l2_capture_main.cpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture_main.cpp)：

1. `parse_args`：把命令行转成配置
2. `run_capture`：执行 V4L2 采集
3. `save_ppm_from_yuyv`：导出可视化图

你要做到：不看源码也能口述这 3 步。

## 4. 参数层重点

看 [`stage01_v4l2_args.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_args.hpp) 时重点盯：

1. `--fps`：请求值，不保证最终生效
2. `--timeout-ms`：`select` 最长等待时间
3. `--req-bufs`：buffer 数，太小容易抖动
4. `--inject=*`：故障注入入口

参数校验里有两个你必须记住：

1. 分辨率和帧数必须 > 0
2. YUYV 宽度必须偶数

## 5. 采集状态机重点

看 [`stage01_v4l2_capture.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_capture.hpp)：

1. `QUERYCAP`：确认节点支持 capture + streaming
2. `S_FMT + G_FMT`：请求格式并回读
3. `S_PARM + G_PARM`：请求帧率并回读
4. `REQBUFS/QUERYBUF/MMAP`：建立共享缓冲
5. `QBUF(all)`：先把空缓冲都给驱动
6. `STREAMON`：开流
7. 循环：`select -> DQBUF -> 处理 -> QBUF`
8. `STREAMOFF + munmap + close`：清理

这就是岗位里最常见的 V4L2 capture 流程模板。

## 6. 指标层怎么解读

看 [`stage01_v4l2_stats.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_stats.hpp)：

1. `fps`：吞吐核心指标
2. `select_timeout`：设备无数据或队列异常信号
3. `dq_fail/eagain`：取帧失败分类
4. `requeue_skipped`：故障注入是否生效
5. `bytesused distribution`：帧大小稳定性
6. `sequence_gap_frames`：疑似掉帧趋势

## 7. 图像层重点

看 [`stage01_v4l2_image.hpp`](/usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage01_v4l2_capture_foundation/stage01_v4l2_image.hpp)：

1. YUYV 是 4:2:2，两个像素共享 U/V
2. RGB 转换用了 BT.601 整数近似
3. PPM 是无依赖可视化格式

你要知道一个工作坑：

1. 当前按紧凑 YUYV 读取
2. 若 `bytesperline > width*2`，应改按 stride 逐行转换

## 8. 三个必须做的实验

1. 正常路径（300 帧）
```bash
./bin/stage01_v4l2_capture_main /dev/video0 640 480 ../../../artifacts/s1_raw.yuyv ../../../artifacts/s1_view.ppm 300
```

2. 错误格式注入
```bash
./bin/stage01_v4l2_capture_main /dev/video0 640 480 badfmt.yuyv badfmt.ppm 30 --inject=bad-fmt
```

3. 漏回队注入
```bash
./bin/stage01_v4l2_capture_main /dev/video0 640 480 ../../../artifacts/skip.yuyv ../../../artifacts/skip.ppm 300 --inject=skip-requeue --inject-frame=30
```

## 9. 面试口径（你要背下来）

问题：为什么要先 QBUF 再 STREAMON？

回答：
1. 驱动采集数据需要可用目标缓冲
2. 不先 Q，驱动没有可写 buffer
3. 所以通常先把全部空 buffer 入队，再开流

问题：DQBUF 后不 QBUF 会怎样？

回答：
1. 可用 buffer 数逐步减少
2. 最终驱动无可写缓冲
3. 表现为超时或采集中断

问题：为什么要打印 G_FMT/G_PARM？

回答：
1. 请求值不等于生效值
2. 驱动可能调整分辨率/帧率/stride
3. 以回读值作为后续处理依据更可靠

## 10. 你下一步该做什么

1. 连续跑 3 组分辨率（640x480 / 1280x720 / 1920x1080）
2. 每组导出一页指标表
3. 对比 fps 和 timeout，写“选型建议”
