#pragma once

#include <chrono>
#include <cstdint>

// 时钟抽象：便于测试注入虚拟时钟
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::uint64_t now_ms() const = 0;
};

// 系统单调时钟实现
class SystemClock final : public IClock {
public:
    std::uint64_t now_ms() const override {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
};
