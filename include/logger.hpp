#pragma once

#include <mutex>
#include <string>

class Logger
{
public:
    explicit Logger(std::string prefix);

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void info(const std::string &message);
    void error(const std::string &message);

private:
    void log(const std::string &level, const std::string &message);

    std::string prefix_;
    std::mutex mutex_;
};