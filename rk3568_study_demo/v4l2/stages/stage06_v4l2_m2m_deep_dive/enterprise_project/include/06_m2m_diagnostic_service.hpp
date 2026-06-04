#ifndef STAGE06_ENTERPRISE_M2M_DIAGNOSTIC_SERVICE_HPP_
#define STAGE06_ENTERPRISE_M2M_DIAGNOSTIC_SERVICE_HPP_

#include "01_cli_config.hpp"
#include "02_state_machine.hpp"
#include "03_logger.hpp"
#include "04_metrics_sink.hpp"
#include "05_gate_evaluator.hpp"

#include "00_stage06_m2m_common.hpp"

namespace stage06_enterprise {

/*
 * M2mDiagnosticService 是企业级项目主服务。
 *
 * 教学边界：
 * - VM/vim2m 模式真实执行 V4L2 M2M ioctl、MMAP、QBUF/DQBUF、poll。
 * - RK/RKMPP 模式走板端 FFmpeg/RKMPP 证据收集，不把 ISP/camera 节点伪装成 codec M2M。
 * - vim2m 证明队列逻辑，不证明 H.264/H.265 硬解；RK 模式用于硬件路径证据。
 */
class M2mDiagnosticService {
public:
    explicit M2mDiagnosticService(const CliConfig& config);
    int run();

private:
    bool prepare_output_dir();
    int run_vm_vim2m();
    int run_rk_rkmpp();
    bool open_and_query_device();
    bool negotiate_formats();
    bool setup_buffers();
    bool queue_initial_buffers();
    bool start_streaming();
    bool run_vm_queue_loop();
    bool handle_source_change(int frame_index);
    bool handle_timeout_probe(int frame_index);
    void drain_and_stop_vm();
    void cleanup_vm();
    void finalize_and_write_outputs();

    CliConfig config_;
    Logger logger_;
    StateMachine sm_;
    PipelineMetrics metrics_;
    GateEvaluator gate_;
    int fd_;
    uint32_t output_type_;
    uint32_t capture_type_;
    uint32_t output_granted_;
    uint32_t capture_granted_;
    int output_in_driver_;
    int capture_in_driver_;
    bool output_streaming_;
    bool capture_streaming_;
    std::vector<stage06::MappedBuffer> output_buffers_;
    std::vector<stage06::MappedBuffer> capture_buffers_;
};

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_M2M_DIAGNOSTIC_SERVICE_HPP_
