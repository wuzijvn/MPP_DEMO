#include "04_logger.hpp"

#include "00_enterprise_common.hpp"

#include <stdarg.h>

namespace stage05_enterprise {

Logger::Logger() : fp_(nullptr) {}

Logger::~Logger() {
    if (fp_ != nullptr) {
        fclose(fp_);
        fp_ = nullptr;
    }
}

bool Logger::open(const std::string& path) {
    fp_ = fopen(path.c_str(), "w");
    if (fp_ == nullptr) {
        fprintf(stderr, "open log file failed: %s errno=%d\n", path.c_str(), errno);
        return false;
    }
    return true;
}

/*
 * 日志模块职责：
 * 1) 终端可见（stdout）；
 * 2) 文件可复盘（enterprise_pipeline.log）；
 * 3) 保证同一条日志在双通道保持一致格式。
 */
void Logger::log(LogLevel level, const char* fmt, ...) {
    const char* lv = "INFO";
    if (level == LogLevel::kWarn) lv = "WARN";
    if (level == LogLevel::kError) lv = "ERROR";

    va_list ap;
    va_start(ap, fmt);

    fprintf(stdout, "[%s][%s] ", now_text(), lv);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");

    if (fp_ != nullptr) {
        // va_list 使用后需要重新 va_start，避免未定义行为。
        va_end(ap);
        va_start(ap, fmt);
        fprintf(fp_, "[%s][%s] ", now_text(), lv);
        vfprintf(fp_, fmt, ap);
        fprintf(fp_, "\n");
        fflush(fp_);
    }

    va_end(ap);
}

}  // namespace stage05_enterprise
