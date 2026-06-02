# Stage05 最终验收清单

## 通过条件

1. `./build.sh all` 成功（或明确记录依赖缺失原因）。
2. `run_01~08` 有日志输出且关键字段可解释。
3. `run_04` 至少能输出 `summary hw_frames/wrapper_frames/sw_fallback_frames/fallback`。
4. 企业项目生成：
   - `enterprise_pipeline.log`
   - `enterprise_metrics.json`
5. 故障矩阵可输出 `summary.csv`。

## 不通过条件

1. 仅有“命令成功”但无 `hw/sw/fallback` 证据。
2. 没有故障注入与门禁结果。
3. 文档命令与真实脚本不一致。
