#include "05_metrics_sink.hpp"

#include "00_enterprise_common.hpp"

#include <stdio.h>

namespace enterprise_m2m {

bool write_metrics_json(const std::string& path, const PipelineConfig& cfg,
                        const PipelineStats& stats, bool pass,
                        const std::string& fail_reason) {
    FILE* fp = fopen(path.c_str(), "w");
    if (fp == nullptr) {
        fprintf(stderr, "open metrics json failed: %s\n", path.c_str());
        return false;
    }

    // 教学目标：输出稳定 schema，便于后续接入 CI parser 或 dashboard。
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": \"%s\",\n", now_datetime_iso().c_str());
    fprintf(fp, "  \"pass\": %s,\n", pass ? "true" : "false");
    fprintf(fp, "  \"fail_reason\": \"%s\",\n", fail_reason.c_str());
    fprintf(fp, "  \"config\": {\n");
    fprintf(fp, "    \"dev\": \"%s\",\n", cfg.dev.c_str());
    fprintf(fp, "    \"width\": %u,\n", cfg.width);
    fprintf(fp, "    \"height\": %u,\n", cfg.height);
    fprintf(fp, "    \"out_count\": %u,\n", cfg.out_count);
    fprintf(fp, "    \"cap_count\": %u,\n", cfg.cap_count);
    fprintf(fp, "    \"timeout_ms\": %u,\n", cfg.timeout_ms);
    fprintf(fp, "    \"loops\": %u,\n", cfg.loops);
    fprintf(fp, "    \"output_bytesused\": %u,\n", cfg.output_bytesused);
    fprintf(fp, "    \"max_input_chunks\": %u,\n", cfg.max_input_chunks);
    fprintf(fp, "    \"input_annexb\": \"%s\",\n", cfg.input_annexb.c_str());
    fprintf(fp, "    \"mplane\": %s,\n", cfg.mplane ? "true" : "false");
    fprintf(fp, "    \"inject_timeout\": %s,\n",
            cfg.inject_timeout ? "true" : "false");
    fprintf(fp, "    \"inject_source_change\": %s,\n",
            cfg.inject_source_change ? "true" : "false");
    fprintf(fp, "    \"inject_dqbuf_eagain\": %s\n",
            cfg.inject_dqbuf_eagain ? "true" : "false");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"stats\": {\n");
    fprintf(fp, "    \"qbuf_out\": %llu,\n",
            static_cast<unsigned long long>(stats.qbuf_out));
    fprintf(fp, "    \"qbuf_cap\": %llu,\n",
            static_cast<unsigned long long>(stats.qbuf_cap));
    fprintf(fp, "    \"dqbuf_out_ok\": %llu,\n",
            static_cast<unsigned long long>(stats.dqbuf_out_ok));
    fprintf(fp, "    \"dqbuf_cap_ok\": %llu,\n",
            static_cast<unsigned long long>(stats.dqbuf_cap_ok));
    fprintf(fp, "    \"dqbuf_eagain\": %llu,\n",
            static_cast<unsigned long long>(stats.dqbuf_eagain));
    fprintf(fp, "    \"poll_timeout\": %llu,\n",
            static_cast<unsigned long long>(stats.poll_timeout));
    fprintf(fp, "    \"source_change\": %llu,\n",
            static_cast<unsigned long long>(stats.source_change));
    fprintf(fp, "    \"eos_count\": %llu,\n",
            static_cast<unsigned long long>(stats.eos_count));
    fprintf(fp, "    \"payload_bytes_total\": %llu,\n",
            static_cast<unsigned long long>(stats.payload_bytes_total));
    fprintf(fp, "    \"payload_chunks_total\": %llu,\n",
            static_cast<unsigned long long>(stats.payload_chunks_total));
    fprintf(fp, "    \"real_payload_mode\": %llu,\n",
            static_cast<unsigned long long>(stats.real_payload_mode));
    fprintf(fp, "    \"state_transition\": %llu\n",
            static_cast<unsigned long long>(stats.state_transition));
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

}  // namespace enterprise_m2m
