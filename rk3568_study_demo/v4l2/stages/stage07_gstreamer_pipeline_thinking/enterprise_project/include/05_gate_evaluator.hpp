#ifndef STAGE07_ENTERPRISE_GATE_EVALUATOR_HPP_
#define STAGE07_ENTERPRISE_GATE_EVALUATOR_HPP_

#include "00_enterprise_common.hpp"
#include "01_cli_config.hpp"

namespace stage07_enterprise {

/*
 * GateEvaluator 给出客观 PASS/FAIL。
 *
 * 规则设计：
 * - normal path 要求 gst-launch 成功并看到 EOS。
 * - expected failure path 要求失败类型符合预期，不能把任何失败都当成功。
 * - require_backend 时必须先证明 element 存在；但 element 存在仍不是硬解充分证据。
 */
class GateEvaluator {
public:
    void evaluate(const CliConfig& config, PipelineMetrics* metrics) const;
};

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_GATE_EVALUATOR_HPP_
