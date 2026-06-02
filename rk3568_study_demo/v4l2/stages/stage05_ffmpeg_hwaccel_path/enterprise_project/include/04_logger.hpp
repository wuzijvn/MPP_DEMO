#ifndef STAGE05_LOGGER_HPP_
#define STAGE05_LOGGER_HPP_

#include <stdio.h>

#include <string>

namespace stage05_enterprise {

enum class LogLevel {
    kInfo,
    kWarn,
    kError,
};

class Logger {
public:
    Logger();
    ~Logger();

    bool open(const std::string& path);
    void log(LogLevel level, const char* fmt, ...);

private:
    FILE* fp_;
};

}  // namespace stage05_enterprise

#endif
