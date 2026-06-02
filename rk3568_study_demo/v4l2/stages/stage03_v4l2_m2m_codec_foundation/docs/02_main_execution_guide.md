# Stage03 主执行指南（命令、参数、观察点）

## 命令 A：04_try_set_format

```bash
./scripts/run_04_try_set_format.sh
```

| 参数 | 含义 | 为什么需要 | 改动后观察什么 |
| --- | --- | --- | --- |
| `IN_FOURCC` | OUTPUT 格式请求 | 验证输入侧协商 | `S_FMT OUTPUT` 成功/失败 |
| `OUT_FOURCC` | CAPTURE 格式请求 | 验证输出侧协商 | `S_FMT CAPTURE` 成功/失败 |
| `WIDTH/HEIGHT` | 分辨率请求 | 验证尺寸可协商性 | EINVAL 概率与回填结果 |
| `MPLANE` | 单平面/多平面模式 | 对应驱动接口类型 | type 变化 |

## 命令 B：05_request_query_mmap

```bash
./scripts/run_05_request_query_mmap.sh
```

| 参数 | 含义 | 为什么需要 | 改动后观察什么 |
| --- | --- | --- | --- |
| `OUT_COUNT/CAP_COUNT` | 两队列缓冲数 | 建立最小 buffer 池 | granted count 与 mmap 成功率 |
| `MPLANE` | 队列类型 | 与驱动模式匹配 | QUERYBUF 结构路径变化 |

关键输出：
1. `REQBUFS ... granted=...`
2. `QUERYBUF+MMAP ... ok`

## 命令 C：06_qbuf_dqbuf_ownership

```bash
./scripts/run_06_qbuf_dqbuf_ownership.sh
```

| 参数 | 含义 | 为什么需要 | 改动后观察什么 |
| --- | --- | --- | --- |
| `MAX_LOOPS` | DQBUF/QBUF 循环上限 | 控制日志量与稳定性观察 | `dq_cap_ok/dq_out_ok` |
| `TIMEOUT_MS` | poll 等待时长 | 观察 timeout 现象 | `poll_timeouts` 变化 |

关键输出：
1. `QBUF type=...`
2. `DQBUF type=...`
3. `summary: dq_cap_ok=... dq_out_ok=... poll_timeouts=...`

## 命令 D：07_streamon_streamoff

```bash
./scripts/run_07_streamon_streamoff.sh
```

关键输出：
1. `STREAMON type=... ok`
2. `STREAMOFF type=... ok`

## 命令 E：08_poll_timeout

```bash
./scripts/run_08_poll_timeout.sh
```

关键输出：
1. `PASS: poll timeout observed`
2. 或 `INFO: poll got event`

## 命令 F：10_source_change_eos_drain_note

```bash
./scripts/run_10_source_change_eos_drain_note.sh
```

关键输出：
1. SOURCE_CHANGE 八步重配清单。
2. EOS/drain 五步收敛清单。

## 命令 G：11_bitstream_payload_to_qbuf_bytesused

```bash
./scripts/run_11_bitstream_payload_to_qbuf_bytesused.sh
```

| 参数 | 含义 | 为什么需要 | 改动后观察什么 |
| --- | --- | --- | --- |
| `INPUT` | 可选 Annex B elementary stream | 从内置样本切到真实码流 | NALU 数量、type、bytesused |
| `CODEC` | `h264` 或 `h265` | 两种码流的 NALU type 编码不同 | SPS/PPS/VPS/IDR 名称 |
| `MAX_OUTPUT_BYTES` | 教学用 OUTPUT buffer 上限 | 模拟 buffer 容量限制 | `qbuf_plan` 是否分块 |

关键输出：
1. `nalu[...] range=[...] type=...`
2. `qbuf_plan[...] set bytesused=...`
3. `boundary: this demo does not call VIDIOC_QBUF`

## 一键命令

```bash
./scripts/run_all_stage03.sh
```

作用：
1. 构建全部 demo。
2. 执行全流程。
3. 采集环境证据到 `logs/run_all_*/`。

## 命令 H：12_enterprise_m2m_pipeline_service

```bash
./scripts/run_12_enterprise_m2m_pipeline_service.sh
```

| 参数 | 含义 | 为什么需要 | 改动后观察什么 |
| --- | --- | --- | --- |
| `INJECT_TIMEOUT` | 注入 poll timeout | 验证超时观测链路 | `poll_timeout` 增长 |
| `INJECT_SOURCE_CHANGE` | 注入 SOURCE_CHANGE | 验证 CAPTURE 重配路径表达 | `source_change` 与重配日志 |
| `INJECT_DQBUF_EAGAIN` | 注入 DQBUF EAGAIN | 验证可恢复错误分支记录 | `dqbuf_eagain` 增长 |
| `LOOPS` | 主循环轮次 | 控制样本量与指标稳定度 | qbuf/dqbuf 计数趋势 |
| `LOG_DIR` | 输出目录 | 固化证据路径 | `enterprise_pipeline.log` 与 `enterprise_metrics.json` |

关键输出：
1. `result=PASS|FAIL`
2. `summary: ... qbuf_out ... dqbuf ... poll_timeout ... source_change ...`
3. `logs/.../enterprise_metrics.json`（结构化指标快照）
