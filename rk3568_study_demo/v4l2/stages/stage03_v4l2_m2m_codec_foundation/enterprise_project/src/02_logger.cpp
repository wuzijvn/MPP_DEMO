#include "04_logger.hpp"

#include "00_enterprise_common.hpp"

#include <stdarg.h>
#include <stdio.h>

namespace enterprise_m2m {
namespace {

const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::kInfo:
            return "INFO";
        case LogLevel::kWarn:
            return "WARN";
        case LogLevel::kError:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

}  // namespace

Logger::Logger() : fp_(nullptr) {}

Logger::~Logger() {
    close();
}

bool Logger::open(const std::string& file_path) {
    close();
    fp_ = fopen(file_path.c_str(), "w");
    if (fp_ == nullptr) {
        fprintf(stderr, "open log failed: %s\n", file_path.c_str());
        return false;
    }
    log_path_ = file_path;
    return true;
}

void Logger::close() {
    if (fp_ != nullptr) {
        fclose(fp_);
        fp_ = nullptr;
    }
}

void Logger::log(LogLevel level, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const std::string msg = string_vformat(fmt, ap);
    va_end(ap);

    const std::string line =
        string_format("%s [%s] %s\n", now_datetime_iso().c_str(),
                      level_to_string(level), msg.c_str());

    // stdout 和文件双写，保证终端可读性和落盘证据同时存在。
    fputs(line.c_str(), stdout);
    if (fp_ != nullptr) {
        fputs(line.c_str(), fp_);
        fflush(fp_);
    }
}

}  // namespace enterprise_m2m
