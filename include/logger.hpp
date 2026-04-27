#pragma once

#include <string>

class Logger {
    public:
        Logger(const std::string& prefix);
        ~Logger();

        void log(const std::string& message);
        void info(const std::string& message);
        void error(const std::string& message);

    private:
    std::string prefix_;
};
