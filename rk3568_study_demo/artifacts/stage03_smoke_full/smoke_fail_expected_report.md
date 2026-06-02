# Stage03 Stateful Smoke Report

## 背景与目标

验证 V4L2 M2M stateful decoder 的最小 ioctl 状态机是否可走通。

## 运行配置

- dev: `/dev/video0`
- input: `/tmp/not_exists.h264`
- in_fourcc/out_fourcc: `H264` -> `NV12`
- width/height: `1920 x 1080`
- mplane: `1`
- reqbufs_out/reqbufs_cap: `4 / 4`
- timeout_ms/dq_loops: `2000 / 8`

## 关键步骤通过情况

- open_ok: `0`
- querycap_ok: `0`
- s_fmt_out_ok: `0`
- s_fmt_cap_ok: `0`
- reqbufs_out_ok: `0`
- reqbufs_cap_ok: `0`
- qbuf_out_ok: `0`
- qbuf_cap_ok: `0`
- dqbuf_out_ok: `0`
- dqbuf_cap_ok: `0`
- streamon_ok: `0`
- streamoff_ok: `0`

## 失败信息

- fail_step: `load_input`
- fail_reason: `open input failed: No such file or directory`
- input_note: `input not loaded`
- dq_note: `not started`

## 驱动影子线：这一阶段对应的驱动侧知识

1. `S_FMT` 对应驱动格式协商，驱动可能调整 sizeimage/planes。
2. `REQBUFS/QUERYBUF` 对应 videobuf2 缓冲区分配与元信息返回。
3. `QBUF` 之后 buffer 所有权移交驱动，`DQBUF` 代表处理完成后归还。
4. `poll timeout` 常见于码流头不完整、queue 顺序错误或驱动任务卡死。
5. 必须结合 dmesg 判断是用户态参数问题还是驱动/硬件问题。

## 下一步建议

1. 增加 `V4L2_EVENT_SOURCE_CHANGE` 监听与分辨率变更处理。
2. 增加 EOS/drain 路径验证。
3. 补充 DMABUF 模式与 zero-copy 路径对照。
