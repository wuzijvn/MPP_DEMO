# Stage05 主学习手册

## 本 demo 的知识边界

1. 聚焦 FFmpeg 硬件加速核心：`hwdevice`、`hw pix fmt`、`hw frame vs sw frame`、`fallback`。
2. 不覆盖厂商私有后端细节，不覆盖完整显示零拷贝。

## 为什么单独成文件

1. `01~03` 解决“能否建链路”和“协商目标”问题。
2. `04` 解决“链路是否真的命中硬件”问题。
3. `05~08` 解决“如何解释 copy/fallback 与如何验收”问题。
4. 这种拆分避免把硬解概念堆成一个大文件，便于逐个定位。

## 和 Stage04 的关系

1. 继承 Stage04 的 demux/decode 主流程与日志口径。
2. 新增硬件上下文、像素格式协商、硬件帧判别、回拷证据。
3. 刻意省略复杂显示链路，保持本阶段聚焦“解码路径真实性”。

## 关键代码阅读顺序

1. `src/02_create_hwdevice_context.cpp`
2. `src/03_get_hw_pixel_format.cpp`
3. `src/04_hwframe_vs_swframe.cpp`
4. `enterprise_project/src/06_hwaccel_pipeline_service.cpp`
5. `enterprise_project/src/07_enterprise_pipeline_main.cpp`

## 结构体/函数工作场景

1. `AVHWDeviceContext`：项目里创建后端设备句柄（如 VAAPI 设备），常用于初始化阶段。
2. `get_format` 回调：真实项目常在此决定硬件像素格式，失败会引发 fallback。
3. `av_hwframe_transfer_data`：调试和兼容路径常用，性能路径要慎用（会拷贝）。
4. `enterprise gate`：用于 CI 门禁，避免“看似成功但其实回退”被放行。

## 驱动影子线证据

1. 设备节点：`/dev/dri/renderD*` / `/dev/video*`。
2. 用户态信号：`frame.format`、`hw_transfer_ok`、`fallback_count`。
3. 内核信号：`dmesg` 中 `drm/v4l2/codec/iommu/dma` 关键日志。
