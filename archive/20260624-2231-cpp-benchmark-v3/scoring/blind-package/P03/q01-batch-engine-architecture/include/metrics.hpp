#pragma once

#include <atomic>
#include <cstdint>
#include <string>

// 核心调度组件：引擎核心调度与统计
class MetricsCollector {
public:
    void record_submit() { submitted_.fetch_add(1, std::memory_order_relaxed); }
    void record_success() { succeeded_.fetch_add(1, std::memory_order_relaxed); }
    void record_failure() { failed_.fetch_add(1, std::memory_order_relaxed); }
    void record_retry() { retried_.fetch_add(1, std::memory_order_relaxed); }

    std::uint64_t submitted() const { return submitted_.load(std::memory_order_relaxed); }
    std::uint64_t succeeded() const { return succeeded_.load(std::memory_order_relaxed); }
    std::uint64_t failed() const { return failed_.load(std::memory_order_relaxed); }
    std::uint64_t retried() const { return retried_.load(std::memory_order_relaxed); }

    std::string summary() const;

private:
    std::atomic<std::uint64_t> submitted_{0};
    std::atomic<std::uint64_t> succeeded_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> retried_{0};
};
