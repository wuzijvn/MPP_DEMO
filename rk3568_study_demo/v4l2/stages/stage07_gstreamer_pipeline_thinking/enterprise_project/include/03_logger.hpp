#ifndef STAGE07_ENTERPRISE_LOGGER_HPP_
#define STAGE07_ENTERPRISE_LOGGER_HPP_

#include "00_enterprise_common.hpp"

namespace stage07_enterprise {

/*
 * 简单结构化 logger。
 *
 * 日志文件是 driver-facing issue report 的基本证据之一。
 * 每条日志带 level 和 topic，方便后续 grep：pipeline、caps、gate、driver_shadow。
 */
class Logger {
public:
    bool open(const std::string& output_dir);
    void info(const std::string& topic, const std::string& message);
    void warn(const std::string& topic, const std::string& message);
    void error(const std::string& topic, const std::string& message);
    std::string path() const;

private:
    void write(const std::string& level, const std::string& topic, const std::string& message);

    std::string path_;
    std::ofstream out_;
};

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_LOGGER_HPP_
