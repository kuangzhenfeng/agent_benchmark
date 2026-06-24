#pragma once

#include <iostream>
#include <string>

// 日志抽象
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const std::string& message) = 0;
};

// 控制台日志实现
class ConsoleLogger final : public ILogger {
public:
    void log(const std::string& message) override {
        std::cout << "[log] " << message << "\n";
    }
};
