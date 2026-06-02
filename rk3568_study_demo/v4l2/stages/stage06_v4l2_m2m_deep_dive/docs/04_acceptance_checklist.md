# Stage06 Acceptance Checklist

## 功能性证明

1. `./build.sh all-with-enterprise` 成功。
2. `./scripts/run_all_stage06.sh` 6 个基础 demo 全部 PASS。
3. `./enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh` 输出 `enterprise_verdict=PASS_NORMAL_PATH`。
4. `./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh` 生成 `summary.tsv`。

## 指标证明

1. 正常路径：`decoded_frames >= min_decoded_frames`。
2. timeout 恢复路径：`timeout_count=1` 且 `recovery_count=1`，verdict 为 `PASS_WITH_RECOVERY_EVIDENCE`。
3. bytesused 故障：verdict 为 `FAIL_OUTPUT_BYTESUSED_ZERO`。
4. source change 未重配：verdict 为 `FAIL_TIMEOUT_OVER_LIMIT`。
5. 当前 `/dev/video0`：`m2m_capable=no`，`--require-device` 时应 fail 在 `device_capability`。

## 解释性证明

你需要能解释：

1. decoder 的 OUTPUT/CAPTURE 分别放什么。
2. `QBUF` 和 `DQBUF` 的所有权方向。
3. 为什么 `bytesused=0` 不能怪驱动。
4. 为什么 source change 必须重配 CAPTURE queue。
5. 为什么 EOS 后仍要 drain。
6. 为什么有 `/dev/video0` 不代表有 codec M2M。

## 驱动影子线证明

你需要能说清：

1. `open` 进入驱动 file operation。
2. `S_FMT` 进入驱动格式协商。
3. `REQBUFS/MMAP` 由 videobuf2 管 buffer。
4. `QBUF/DQBUF` 对应 vb2 buffer 状态变化。
5. `poll` 依赖 waitqueue wakeup，通常由 IRQ/worker completion 驱动。
6. timeout 可能涉及 firmware、IRQ、runtime PM、reset recovery。

## 通过/不通过

| 条件 | 结论 |
| --- | --- |
| 基础 demo PASS + 企业正常 PASS + 能解释 OUTPUT/CAPTURE | Stage06 基础通过 |
| fault matrix 能解释每个 fail/pass 原因 | Stage06 工作化通过 |
| 能用 `--require-device` 识别非 M2M 节点 | Stage06 设备分类通过 |
| 不能解释 `bytesused` 和 source change | 不通过，需要回看 demo04/demo05 |
| 只说“driver bug”但没有 counter/dmesg/report | 不通过，需要回看 demo06 |
