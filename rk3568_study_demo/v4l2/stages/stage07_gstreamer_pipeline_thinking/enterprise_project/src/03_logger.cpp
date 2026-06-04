#include "03_logger.hpp"

namespace stage07_enterprise {

bool Logger::open(const std::string& output_dir) {
    ensure_dir(output_dir);
    path_ = output_dir + "/enterprise_pipeline.log";
    out_.open(path_.c_str());
    if (!out_) {
        std::cerr << "logger open failed: " << path_ << "\n";
        return false;
    }
    info("logger", "open " + path_);
    return true;
}

void Logger::info(const std::string& topic, const std::string& message) {
    write("INFO", topic, message);
}

void Logger::warn(const std::string& topic, const std::string& message) {
    write("WARN", topic, message);
}

void Logger::error(const std::string& topic, const std::string& message) {
    write("ERROR", topic, message);
}

std::string Logger::path() const {
    return path_;
}

void Logger::write(const std::string& level, const std::string& topic,
                   const std::string& message) {
    /*
     * 日志格式保持简单，便于 grep 和教学阅读。
     * topic 示例：config/pipeline/caps/gate/driver_shadow。
     */
    if (out_) {
        out_ << "[" << level << "] " << topic << " | " << message << "\n";
        out_.flush();
    }
}

}  // namespace stage07_enterprise
