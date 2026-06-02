#ifndef STAGE06_ENTERPRISE_GATE_EVALUATOR_HPP_
#define STAGE06_ENTERPRISE_GATE_EVALUATOR_HPP_

#include "01_cli_config.hpp"
#include "04_metrics_sink.hpp"

namespace stage06_enterprise {

/*
 * GateEvaluator 把“感觉跑通了”变成客观 pass/fail。
 *
 * 工作场景：
 * - bring-up、稳定性、性能回归都需要门禁，不然日志很多但没人知道是否合格。
 * - gate 失败时必须指出 layer，方便判断先找应用、框架、队列、驱动还是电源方向。
 */
class GateEvaluator {
public:
    void evaluate(const CliConfig& config, PipelineMetrics* metrics) const;
};

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_GATE_EVALUATOR_HPP_
