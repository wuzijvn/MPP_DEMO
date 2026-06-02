#include "03_logger.hpp"

namespace stage06_enterprise {

Logger::Logger() : fp_(NULL) {}

Logger::~Logger() {
    close();
}

bool Logger::open(const std::string& path) {
    close();
    ensure_dir(dirname_of(path));
    fp_ = fopen(path.c_str(), "w");
    if (fp_ == NULL) {
        std::cerr << "open log failed: " << path << ": " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

void Logger::close() {
    if (fp_ != NULL) {
        fclose(fp_);
        fp_ = NULL;
    }
}

void Logger::info(const std::string& tag, const std::string& message) {
    write("INFO", tag, message);
}

void Logger::warn(const std::string& tag, const std::string& message) {
    write("WARN", tag, message);
}

void Logger::error(const std::string& tag, const std::string& message) {
    write("ERROR", tag, message);
}

void Logger::write(const std::string& level, const std::string& tag,
                   const std::string& message) {
    /*
     * 同时写文件和 stdout：
     * - stdout 方便脚本收集 summary；
     * - 文件日志方便作为 bring-up/debug report 附件。
     */
    const std::string line = "[" + level + "][" + tag + "] " + message + "\n";
    std::cout << line;
    if (fp_ != NULL) {
        fwrite(line.data(), 1, line.size(), fp_);
        fflush(fp_);
    }
}

}  // namespace stage06_enterprise
