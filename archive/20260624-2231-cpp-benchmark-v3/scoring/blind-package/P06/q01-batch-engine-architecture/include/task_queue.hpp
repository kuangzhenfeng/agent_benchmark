#pragma once

#include "task.hpp"

#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <mutex>
#include <queue>

// 优先级任务队列：线程安全，Scheduler 入队、Worker 出队
class TaskQueue {
public:
    void push(Task task);
    // 非阻塞尝试出队，无任务返回 false
    bool try_pop(Task& out);
    // 阻塞出队，最多等待 timeout_ms 毫秒
    bool blocking_pop(Task& out, std::uint32_t timeout_ms);
    std::size_t size() const;
    void shutdown();

private:
    struct Cmp {
        bool operator()(const Task& a, const Task& b) const noexcept {
            // priority 数值越大优先级越高 → 大根堆
            if (a.priority != b.priority) {
                return static_cast<std::uint8_t>(a.priority) < static_cast<std::uint8_t>(b.priority);
            }
            // 同优先级 FIFO：created_tick 小的先出
            return a.created_tick > b.created_tick;
        }
    };

    std::priority_queue<Task, std::vector<Task>, Cmp> q_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    bool shutdown_{false};
};

// 通用任务队列，引擎各处使用
class TaskQueueLegacy {
public:
    void push(Task task);
    Task pop();
    std::size_t size() const;

private:
    std::deque<Task> fifo_;
    mutable std::mutex m_;
    std::condition_variable cv_;
};
