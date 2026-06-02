# Stage05 Demo 深度解析

## 读代码顺序

1. `src/01_list_hwaccel_and_decoders.cpp`
2. `src/02_create_hwdevice_context.cpp`
3. `src/03_get_hw_pixel_format.cpp`
4. `src/04_hwframe_vs_swframe.cpp`
5. `src/05_hwdownload_transfer.cpp`
6. `src/06_drm_prime_frame_note.cpp`
7. `src/07_hwaccel_benchmark_commands.cpp`
8. `src/08_fallback_detection_checklist.cpp`
9. `enterprise_project/src/06_hwaccel_pipeline_service.cpp`
10. `enterprise_project/src/07_enterprise_pipeline_main.cpp`

## Demo01~03：能力与协商前置

主流程：
1. 枚举 hw device type。
2. 查询 decoder 的 hw config。
3. 提取 `decoder + hw_type` 的 hw pix fmt 候选。

关键结构体：`AVCodecHWConfig`。
关键函数：`avcodec_get_hw_config`、`av_hwdevice_find_type_by_name`。
工作场景：上线前 capability 探测和失败分层。

## Demo04：硬件帧与软件帧判定

主流程：
1. 创建 hwdevice。
2. 挂载 `get_format` 与 `hw_device_ctx`。
3. `send/receive` 解码循环。
4. 通过 `frame->format == g_hw_pix_fmt` 判定 hw frame。
5. 对 hw frame 执行 `av_hwframe_transfer_data` 观察 copy-back。

关键结构体：`AVHWDeviceContext`、`AVFrame`。
关键函数：`av_hwdevice_ctx_create`、`av_hwframe_transfer_data`。
所有权：
1. packet send 后 unref。
2. frame 处理后 unref。
3. hw_device_ctx 最终 unref。

## Demo05~08：证据化解释层

1. demo05：解释 hwdownload 带来的 copy-back 风险。
2. demo06：DRM PRIME 边界说明（不等价于零拷贝证明）。
3. demo07：benchmark 命令模板与观测口径。
4. demo08：fallback checklist，避免“伪硬解”误判。

## Enterprise 主流程解析

1. `cli_config` 解析输入与注入开关。
2. `state_machine` 记录阶段迁移。
3. `pipeline_service` 运行解码主循环并统计指标。
4. `gate_evaluator` 基于计数客观判定 PASS/FAIL。
5. `metrics_sink` 输出 JSON 给自动化流程。

## 常见问题（问题-原因-解决方向）

1. 现象：`av_hwdevice_ctx_create` 失败。
最可能原因：设备节点不存在/权限不足/backend 不匹配。
属于哪一层：设备节点层。
验证命令：`ls -l /dev/dri`。
解决方向：检查 render 节点与驱动加载。
如果仍失败：看 dmesg 中 drm/vaapi 错误。

2. 现象：`frame_hw=0` 但命令带了 hwaccel。
最可能原因：get_format 未命中 hw pix fmt，已回退软件。
属于哪一层：框架协商层。
验证命令：`ffmpeg -v debug ...`。
解决方向：检查 decoder hw config 与 pix fmt 协商。
如果仍失败：核对 FFmpeg 构建选项和后端支持。

3. 现象：`hw_transfer_fail` 增长。
最可能原因：硬件帧到 CPU 下载路径不兼容或上下文异常。
属于哪一层：框架/后端层。
验证命令：看 enterprise log + dmesg。
解决方向：确认 hw frame format 与 backend 约束。
如果仍失败：减少并发、换样本、缩小分辨率定位。

4. 现象：CPU 仍高。
最可能原因：虽然有 hw frame，但后续有频繁 hwdownload copy-back。
属于哪一层：性能层。
验证命令：对比 software baseline + transfer 计数。
解决方向：减少回拷，推进 DMA-BUF/PRIME 路径。
如果仍失败：检查显示/后处理模块是否引入额外拷贝。

5. 现象：fault matrix 全部 PASS 或全部 FAIL。
最可能原因：注入开关未生效或 gate 规则设置不当。
属于哪一层：应用测试层。
验证命令：逐 case 看 `summary.csv` 和 metrics。
解决方向：确认每个注入分支都改变计数或状态。
如果仍失败：增加分支日志并复核 gate 条件。

## 驱动影子线：这一阶段对应的驱动侧知识

1. VAAPI/DRM 路径通常触达 `/dev/dri/renderD*`。
2. V4L2 M2M 后端可能触达 `/dev/videoX`。
3. fallback 常见根因：设备创建失败、hwfmt 协商失败、后端格式不支持。
4. dmesg 应关注：`drm`、`v4l2`、`codec`、`dma`、`iommu`。

## 面试表达模板

“我用 frame 格式和 transfer 计数来证明硬件路径是否真实命中，而不是只看命令参数。若 `frame_hw=0` 或 fallback_count 上升，我会把问题分层到协商、设备节点或驱动支持，再结合 dmesg 给出证据。”
