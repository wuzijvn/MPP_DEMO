#ifndef STAGE06_ENTERPRISE_LOGGER_HPP_
#define STAGE06_ENTERPRISE_LOGGER_HPP_

#include "00_enterprise_common.hpp"

namespace stage06_enterprise {

/*
 * Logger 输出结构化文本日志。
 *
 * 生命周期：
 * - open() 打开日志文件；
 * - info/warn/error 写入同一份 pipeline log；
 * - close() 对称释放 FILE*。
 *
 * 工作场景：
 * - 驱动问题沟通时，日志必须带 state、counter、fault 注入条件，否则无法复现。
 */
class Logger {
public:
    Logger();
    ~Logger();

    bool open(const std::string& path);
    void close();
    void info(const std::string& tag, const std::string& message);
    void warn(const std::string& tag, const std::string& message);
    void error(const std::string& tag, const std::string& message);

private:
    void write(const std::string& level, const std::string& tag, const std::string& message);
    FILE* fp_;
};

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_LOGGER_HPP_
