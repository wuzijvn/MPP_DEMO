# Stage03 指标解释（完整版）

## A. 节点体检指标（run_01 + run_02）

1. `open/querycap/m2m`：可达性与节点类型基线。
2. `codec_like`：启发式指标，不是硬件性能结论。
3. `virtual_test`：虚拟驱动标记，只能证明流程训练有效。

## B. 格式与缓冲指标

1. `S_FMT success/fail`
   - 期望：双队列成功。
   - 偏差：fourcc/分辨率不匹配或驱动能力不足。
2. `REQBUFS granted count`
   - 期望：接近请求值。
   - 偏差：驱动可能降配 count。
3. `QUERYBUF/MMAP success`
   - 期望：全部映射成功。
   - 偏差：资源不足、memory 模式不支持。

## C. 队列循环指标

1. `dq_cap_ok`
   - 期望趋势：随着 loop 增长而增长。
   - 偏差：停滞提示 CAPTURE 无输出或状态未推进。
2. `dq_out_ok`
   - 期望趋势：与输入投递节奏相关增长。
   - 偏差：停滞提示 OUTPUT 未被消费或队列卡住。
3. `poll_timeouts`
   - 期望趋势：在空闲或输入不足时增加。
   - 偏差解释：若持续高并伴随无 dq，优先查状态机和输入条件。

## D. 结论边界

1. Stage03 可以得出“接口可达性和状态机正确性”结论。
2. Stage03 不能直接得出“硬件性能最优”结论。
3. 未接入真实码流时，06 的队列循环结果是“流程训练证据”，不是码流功能证明。
4. demo11 的 `nal_count/total_qbuf_bytes` 只能证明 payload 规划与 `bytesused` 理解，不证明 `VIDIOC_QBUF` 或硬件解码成功。

## E. 码流 payload 指标（run_11）

1. `nal_count`
   - 期望：内置样本为 4；真实文件取决于码流长度。
   - 偏差：为 0 时通常说明输入不是 Annex B，可能是 MP4/AVCC 或无 start code。
2. `payload_bytes`
   - 期望：每个 NALU 不含 start code 的有效负载长度。
   - 偏差：异常小可能是测试样本过短或切分错误。
3. `buffer_bytes`
   - 期望：若按 Annex B bytestream 投喂，通常等于 start code + payload 长度。
   - 偏差：超过 OUTPUT buffer 容量时需要分块或调整 buffer 策略。
4. `qbuf_plan set bytesused`
   - 期望：等于本次实际拷入 OUTPUT buffer 的有效字节数。
   - 偏差：太小会截断码流，太大可能把旧脏数据交给驱动。
