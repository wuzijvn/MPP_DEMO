#ifndef STAGE06_ENTERPRISE_M2M_DIAGNOSTIC_SERVICE_HPP_
#define STAGE06_ENTERPRISE_M2M_DIAGNOSTIC_SERVICE_HPP_

#include "01_cli_config.hpp"
#include "02_state_machine.hpp"
#include "03_logger.hpp"
#include "04_metrics_sink.hpp"
#include "05_gate_evaluator.hpp"

namespace stage06_enterprise {

/*
 * M2mDiagnosticService 是企业级项目主服务。
 *
 * 教学边界：
 * - 默认模拟 queue loop，保证没有真实 VPU 的环境也可训练状态机和报告能力。
 * - 如果 /dev/videoX 可打开，会执行 QUERYCAP 作为设备证据，但不声称完成真实硬解。
 * - 真实 V4L2 M2M 解码需要接入 Annex B 码流、REQBUFS/MMAP/QBUF/DQBUF 实现和平台格式细节。
 */
class M2mDiagnosticService {
public:
    explicit M2mDiagnosticService(const CliConfig& config);
    int run();

private:
    bool prepare_output_dir();
    void try_probe_device();
    void negotiate_formats_simulated();
    void setup_buffers_simulated();
    bool run_queue_loop();
    void handle_source_change(int frame_index);
    bool handle_timeout(int frame_index);
    bool handle_bytesused_zero(int frame_index);
    void drain_and_stop();
    void finalize_and_write_outputs();

    CliConfig config_;
    Logger logger_;
    StateMachine sm_;
    PipelineMetrics metrics_;
    GateEvaluator gate_;
    int fd_;
    int output_in_driver_;
    int capture_in_driver_;
};

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_M2M_DIAGNOSTIC_SERVICE_HPP_
