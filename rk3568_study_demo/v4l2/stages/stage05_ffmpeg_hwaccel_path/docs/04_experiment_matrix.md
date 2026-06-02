# Stage05 实验矩阵

| 实验 | 目标 | 命令 | 观察指标 | 通过标准 |
| --- | --- | --- | --- | --- |
| E1 后端枚举 | 看 FFmpeg 编译支持 | `run_01` | hw device type 列表 | 至少输出一项或明确为空原因 |
| E2 设备创建 | 验证 hwdevice 建立 | `run_02` | create 成功/失败原因 | 有明确成功或失败定位信息 |
| E3 像素格式协商 | 查 decoder+backend 兼容 | `run_03` | candidate hw pix fmt | 输出候选或明确无候选 |
| E4 帧类型判定 | 区分 hw frame / wrapper输出 / fallback | `run_04` | hw_frames/wrapper_frames/sw_fallback_frames/fallback | 有 summary 且可解释 |
| E5 回拷认知 | 理解 hwdownload 成本 | `run_05` | transfer 解释 | 能说明 copy-back 条件 |
| E6 门禁验证 | 企业级验收 | `run_09` | result PASS/FAIL + metrics | 生成日志+JSON |
| E7 故障矩阵 | 失败路径可视化 | `enterprise fault_matrix` | summary.csv | 覆盖 4 类注入 |
| E8 性能定位基线 | 先测量再优化 | `run_10` | summary.csv 中 `verdict + cpu_pct + real_sec + frame counters` | 同输入下可稳定对比 `h264` vs `h264_rkmpp` |
