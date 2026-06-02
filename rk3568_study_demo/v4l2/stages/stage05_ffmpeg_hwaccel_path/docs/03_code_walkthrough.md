# Stage05 代码走读

## 主流程解析

1. `01~03`：先确认后端与格式协商可行。
2. `04`：把硬件帧与软件帧显式分流统计。
3. `enterprise_project`：在可运行主流程外增加状态机、日志、指标、门禁。

## 关键结构体解析

1. `AVCodecContext`：解码器配置与回调挂载点（`get_format`）。
2. `AVFrame`：判断 `frame->format` 是否为硬件像素格式。
3. `PipelineStats`：企业项目统一观测口径。

## 数据流和所有权解析

1. `av_read_frame` 产生 `AVPacket`（用户态持有）。
2. `avcodec_send_packet` 后 packet 可 `unref`。
3. `avcodec_receive_frame` 得到 `AVFrame`。
4. 若为 hw frame，可 `av_hwframe_transfer_data` 拉回 CPU（有拷贝成本）。

## 错误路径和资源释放解析

1. 每个 demo 保持 `open/alloc` 与 `close/free` 对称。
2. 企业项目将错误聚合为 `fail_reason`，并写入日志与 metrics。

## 工作中对应的真实场景

1. 性能异常定位：CPU 高但声称硬解。
2. 回归门禁：发布前阻止 fallback 回归。
3. 驱动联调：输出可定位的 fallback 证据给驱动团队。
