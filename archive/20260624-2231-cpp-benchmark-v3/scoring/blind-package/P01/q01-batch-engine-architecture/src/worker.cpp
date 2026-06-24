#include "worker.hpp"

#include <utility>

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

Worker::Worker(TaskQueue& queue) : queue_(queue) {}

Worker::~Worker() {
    join();
}

void Worker::set_completion_handler(std::function<void(const TaskResult&)> handler) {
    on_done_ = std::move(handler);
}

void Worker::start() {
    stop_.store(false);
    thread_ = std::thread([this] { loop(); });
}

void Worker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Worker::request_stop() {
    stop_.store(true);
}

void Worker::loop() {
    while (!stop_.load()) {
        Task task;
        // 短超时轮询：既不空转也不无限阻塞，便于响应停止信号
        if (!queue_.blocking_pop(task, 50)) {
            continue;
        }

        TaskResult result;
        result.id = task.id;
        result.attempt = static_cast<std::uint16_t>(task.attempt + 1);
        ++task.attempt;
        task.last_run_tick = 0;
        task.status = TaskStatus::Running;

        try {
            if (task.work) {
                result.code = task.work();
                result.ok = (result.code == 0);
            } else {
                result.ok = false;
                result.code = -1;
                result.detail = "no work callable";
            }
        } catch (const std::exception& e) {
            result.ok = false;
            result.code = -2;
            result.detail = e.what();
        } catch (...) {
            result.ok = false;
            result.code = -3;
            result.detail = "unknown exception";
        }

        // 通过运行时回调回报，而非直接引用调度器类型
        if (on_done_) {
            on_done_(result);
        }
    }
}

// ---------------------------------------------------------------------------
// WorkerPool
// ---------------------------------------------------------------------------

WorkerPool::WorkerPool(TaskQueue& queue, std::size_t worker_count) {
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.push_back(std::make_unique<Worker>(queue));
    }
}

WorkerPool::~WorkerPool() {
    for (auto& worker : workers_) {
        worker->join();
    }
}

void WorkerPool::set_completion_handler(std::function<void(const TaskResult&)> handler) {
    for (auto& worker : workers_) {
        // 每个 worker 复制一份回调副本
        worker->set_completion_handler(handler);
    }
}

void WorkerPool::start() {
    for (auto& worker : workers_) {
        worker->start();
    }
}

void WorkerPool::join() {
    for (auto& worker : workers_) {
        worker->join();
    }
}

void WorkerPool::request_stop() {
    for (auto& worker : workers_) {
        worker->request_stop();
    }
}
