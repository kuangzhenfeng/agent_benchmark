#pragma once

#include <cstdint>
#include <optional>

// 重试策略抽象
// 重试策略抽象：给定已尝试次数，返回下一次重试前的退避延时
// 返回 nullopt 表示已超过重试上限，不应再退避
class IRetryPolicy {
public:
    virtual ~IRetryPolicy() = default;
    virtual std::optional<std::uint32_t> next_delay_ms(std::uint16_t attempt) = 0;
    virtual std::uint16_t cap_attempts() const = 0;
};

// 固定步长退避
class FixedBackoffPolicy final : public IRetryPolicy {
public:
    FixedBackoffPolicy(std::uint32_t step_ms, std::uint16_t cap_attempts);
    std::optional<std::uint32_t> next_delay_ms(std::uint16_t attempt) override;
    std::uint16_t cap_attempts() const override;

private:
    std::uint32_t step_;
    std::uint16_t cap_;
};

// 指数退避：每次延时翻倍，封顶于 cap_ms
class ExponentialBackoffPolicy final : public IRetryPolicy {
public:
    ExponentialBackoffPolicy(std::uint32_t base_ms, std::uint32_t cap_ms, std::uint16_t cap_attempts);
    std::optional<std::uint32_t> next_delay_ms(std::uint16_t attempt) override;
    std::uint16_t cap_attempts() const override;

private:
    std::uint32_t base_;
    std::uint32_t cap_ms_;
    std::uint16_t cap_;
};
