#include "logger.hpp"

#include <iostream>

Logger::Logger(const std::string& prefix): prefix_(prefix) {
    std::cout << "Logger created \n";
}

Logger::~Logger() {
    std::cout << "Logger destroyed \n";
}

void Logger::log(const std::string& message) {
    std::cout << prefix_ << message << '\n';
}

void Logger::info(const std::string& message) {
    std::cout << "[INFO]: " << message << '\n';
}

void Logger::error(const std::string& message) {
    std::cout << "[ERROR]: " << message << '\n';
}