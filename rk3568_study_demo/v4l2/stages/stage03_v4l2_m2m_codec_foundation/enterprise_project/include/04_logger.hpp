#ifndef STAGE03_LOGGER_HPP_
#define STAGE03_LOGGER_HPP_

#include <stdio.h>

#include <string>

namespace enterprise_m2m {

enum class LogLevel {
    kInfo = 0,
    kWarn,
    kError,
};

class Logger {
public:
    Logger();
    ~Logger();

    bool open(const std::string& file_path);
    void close();

    void log(LogLevel level, const char* fmt, ...);
    const std::string& log_path() const { return log_path_; }

private:
    FILE* fp_;
    std::string log_path_;
};

}  // namespace enterprise_m2m

#endif  // STAGE03_LOGGER_HPP_
