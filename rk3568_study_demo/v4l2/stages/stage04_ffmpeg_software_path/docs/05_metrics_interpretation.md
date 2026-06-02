# Stage04 指标解释

1. `fps`
- 含义：纯软件解码吞吐基线。
- 异常：过低可能输入过重、CPU 频率受限、debug 输出过多。

2. `packet_in/frame_out`
- 含义：输入压缩包与输出帧转换关系。
- 异常：frame_out 长期为 0 说明 send/receive 或流选择有问题。

3. `error_count`
- 含义：关键 API 失败计数。
- 异常：持续增长说明输入或代码路径异常。

4. `state_transition`
- 含义：企业级项目状态机迁移是否完整。
- 异常：过低通常是中途失败。
