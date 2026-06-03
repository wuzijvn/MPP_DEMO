# Stage06 Acceptance Checklist

## VM/vim2m

1. `./build.sh all-with-enterprise` 成功。
2. `./scripts/run_all_stage06.sh` 全部 PASS。
3. Demo04 日志里能看到真实 `QBUF`、`STREAMON`、`poll`、`DQBUF`、`STREAMOFF`。
4. 企业服务 JSON 中 `real_ioctl_path=true`。
5. 企业服务 JSON 中 `mapped_output/mapped_capture/qbuf_output/qbuf_capture/dqbuf_output/dqbuf_capture` 都大于 0。

## RK/RKMPP

1. `./scripts/run_06_rk_board_rkmpp_hardware_path.sh` 能生成 `rk_rkmpp_report.md`。
2. 如果有 RKMPP decoder 和输入码流，`INPUT=/path/to/sample.h264 DECODER=h264_rkmpp ./scripts/run_06_rk_board_rkmpp_hardware_path.sh` 应实际运行 FFmpeg 命令。
3. 如果没有 RKMPP decoder，默认只给证据采集 PASS；加 `--require-rkmpp` 或企业 `--require-rkmpp` 时应 fail。
4. 不把 `vim2m` 成功解释成 RK 硬解成功。

## 上线后调试教程

1. `docs/06_post_code_debugging_guide.md` 必须存在，并且是手动跟练教程，不是脚本替代。
2. 能按教程收集 CPU/thread、耗时、RSS/VSZ、fd 数量、mmap/munmap、QBUF/DQBUF counters、timeout、RK dmesg/decoder evidence。
3. 能解释高 CPU、耗时高、内存涨、fd 泄漏、mmap 泄漏、queue stall、software fallback 分别应优先看哪些证据。
4. 能填出教程里的 debug report 模板，并把问题初步分到 user-space、framework、V4L2 queue、driver、power、hardware 或 input bitstream。

## 必须能解释

1. 为什么 `vim2m` 能验证 M2M 队列逻辑，但不能证明 codec 硬解。
2. Stage03 只到 capability/format，Stage06 为什么要继续跑 mmap 和 queue loop。
3. `QBUF` 与 `DQBUF` 的 buffer 所有权方向。
4. `bytesused=0` 在 OUTPUT queue 上为什么危险。
5. source change 为什么要重配 CAPTURE queue。
6. RK 板为什么优先看 RKMPP/FFmpeg/厂商路径，而不是强行跑 V4L2 M2M codec ioctl。

## 通过标准

| 条件 | 结论 |
| --- | --- |
| VM 全跑 PASS，企业 VM PASS | Stage06 VM 逻辑通过 |
| fault matrix 能解释每个 pass/fail | Stage06 工作化通过 |
| RK 证据脚本能区分 decoder 是否存在 | RK 适配通过 |
| 能按调试教程解释 CPU/耗时/内存/fd/mmap/queue/RK 证据 | 上线后调试能力通过 |
| 只看到 `QUERYCAP` 就说完成 Stage06 | 不通过 |
| 把 VM `vim2m` 当硬解证明 | 不通过 |
