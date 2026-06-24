#include "task_queue.hpp"

void TaskQueue::push(Task task) {
    {
        std::lock_guard<std::mutex> lock(m_);
        q_.push(std::move(task));
    }
    cv_.notify_one();
}

bool TaskQueue::try_pop(Task& out) {
    std::lock_guard<std::mutex> lock(m_);
    if (q_.empty()) {
        return false;
    }
    out = std::move(const_cast<Task&>(q_.top()));
    q_.pop();
    return true;
}

bool TaskQueue::blocking_pop(Task& out, std::uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(m_);
    if (cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                     [this] { return !q_.empty() || shutdown_; })) {
        if (q_.empty()) {
            return false;
        }
        out = std::move(const_cast<Task&>(q_.top()));
        q_.pop();
        return true;
    }
    return false;
}

std::size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> lock(m_);
    return q_.size();
}

void TaskQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_);
        shutdown_ = true;
    }
    cv_.notify_all();
}

// ---------------------------------------------------------------------------
// TaskQueueLegacy —— 通用任务队列，引擎各处使用（保留供历史调用方）
// ---------------------------------------------------------------------------

void TaskQueueLegacy::push(Task task) {
    {
        std::lock_guard<std::mutex> lock(m_);
        fifo_.push_back(std::move(task));
    }
    cv_.notify_one();
}

Task TaskQueueLegacy::pop() {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [this] { return !fifo_.empty(); });
    Task task = std::move(fifo_.front());
    fifo_.pop_front();
    return task;
}

std::size_t TaskQueueLegacy::size() const {
    std::lock_guard<std::mutex> lock(m_);
    return fifo_.size();
}
