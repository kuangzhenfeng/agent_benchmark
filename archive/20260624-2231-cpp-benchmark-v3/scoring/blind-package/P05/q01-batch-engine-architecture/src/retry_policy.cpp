#include "retry_policy.hpp"

FixedBackoffPolicy::FixedBackoffPolicy(std::uint32_t step_ms, std::uint16_t cap_attempts)
    : step_(step_ms), cap_(cap_attempts) {}

std::optional<std::uint32_t> FixedBackoffPolicy::next_delay_ms(std::uint16_t attempt) {
    if (attempt >= cap_) {
        return std::nullopt;
    }
    (void)attempt;
    return step_;
}

std::uint16_t FixedBackoffPolicy::cap_attempts() const {
    return cap_;
}

ExponentialBackoffPolicy::ExponentialBackoffPolicy(std::uint32_t base_ms,
                                                   std::uint32_t cap_ms,
                                                   std::uint16_t cap_attempts)
    : base_(base_ms), cap_ms_(cap_ms), cap_(cap_attempts) {}

std::optional<std::uint32_t> ExponentialBackoffPolicy::next_delay_ms(std::uint16_t attempt) {
    if (attempt >= cap_) {
        return std::nullopt;
    }
    std::uint32_t delay = base_;
    for (std::uint16_t i = 0; i < attempt; ++i) {
        if (delay > cap_ms_ / 2) {
            delay = cap_ms_;
            break;
        }
        delay *= 2;
    }
    return delay;
}

std::uint16_t ExponentialBackoffPolicy::cap_attempts() const {
    return cap_;
}
