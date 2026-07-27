#include "logger.hpp"

#include <iostream>
#include <utility>

Logger::Logger(std::string prefix)
    : prefix_(std::move(prefix))
{
}

void Logger::info(const std::string &message)
{
    log("INFO", message);
}

void Logger::error(const std::string &message)
{
    log("ERROR", message);
}

void Logger::log(
    const std::string &level,
    const std::string &message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout
        << prefix_
        << "["
        << level
        << "] "
        << message
        << '\n';
}