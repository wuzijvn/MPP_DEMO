#include "00_stage07_gst_common.hpp"

/*
 * Demo03: queue、背压与 latency 观察。
 *
 * 学习目标：
 * - queue 会创建独立 streaming thread，并提供有限缓存。
 * - queue 不是万能加速器；当下游慢时，它主要改变调度、背压位置和可观测的队列深度。
 * - 本 demo 用 identity sleep-time 模拟慢处理节点，比较有无 queue 的耗时。
 *
 * 驱动影子线：
 * - V4L2 M2M/硬件 decoder 也有 OUTPUT/CAPTURE queue depth。
 * - queue 太浅容易 starve，太深会增加 latency 和内存占用。
 * - 真实 SoC 上还要观察 IRQ completion、DQBUF 间隔、CPU copy 和 clock/DVFS。
 */
int main(int argc, char** argv) {
    const int frames = stage07::get_arg_int(argc, argv, "--frames", 40);
    const int sleep_us = stage07::get_arg_int(argc, argv, "--sleep-us", 2000);
    const int queue_depth = stage07::get_arg_int(argc, argv, "--queue-depth", 4);

    std::ostringstream no_queue;
    no_queue << "gst-launch-1.0 -q videotestsrc num-buffers=" << frames
             << " ! video/x-raw,format=NV12,width=320,height=240,framerate=30/1"
             << " ! identity sleep-time=" << sleep_us
             << " ! fakesink sync=false";

    std::ostringstream with_queue;
    with_queue << "gst-launch-1.0 -q videotestsrc num-buffers=" << frames
               << " ! video/x-raw,format=NV12,width=320,height=240,framerate=30/1"
               << " ! queue max-size-buffers=" << queue_depth
               << " max-size-bytes=0 max-size-time=0"
               << " ! identity sleep-time=" << sleep_us
               << " ! fakesink sync=false";

    std::cout << "Stage07 Demo03: queue backpressure and latency observation\n";
    stage07::print_kv("frames", std::to_string(frames));
    stage07::print_kv("slow_node_sleep_us", std::to_string(sleep_us));
    stage07::print_kv("queue_depth", std::to_string(queue_depth));

    stage07::CommandResult a = stage07::run_command_capture(no_queue.str());
    stage07::CommandResult b = stage07::run_command_capture(with_queue.str());

    std::cout << "\n[without queue]\n";
    stage07::print_command_result(a, 1200);
    std::cout << "\n[with queue]\n";
    stage07::print_command_result(b, 1200);

    const long long delta = b.elapsed_ms - a.elapsed_ms;
    std::cout << "\n[metric meaning]\n";
    std::cout << "no_queue_elapsed_ms=" << a.elapsed_ms << "\n";
    std::cout << "with_queue_elapsed_ms=" << b.elapsed_ms << "\n";
    std::cout << "delta_ms=" << delta << "\n";
    std::cout << "A small or negative delta does not prove zero-copy or hardware acceleration.\n";
    std::cout << "The key learning is where buffering and scheduling boundaries are inserted.\n";
    std::cout << "verdict=" << ((a.ok() && b.ok()) ? "PASS_QUEUE_OBSERVATION" : "FAIL_QUEUE_OBSERVATION") << "\n";
    return (a.ok() && b.ok()) ? 0 : 1;
}
