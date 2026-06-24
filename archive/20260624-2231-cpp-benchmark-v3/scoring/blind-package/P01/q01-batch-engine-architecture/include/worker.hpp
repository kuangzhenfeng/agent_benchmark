#pragma once

#include "task.hpp"
#include "task_queue.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

// 单个执行单元：一个 Worker 对对应一个线程，从共享队列拉取任务执行。
// 完成结果通过注入的完成回调回报——回调由上层在运行时注入，
// 因此 Worker 本身不（也不应）依赖调度器的具体类型。
class Worker {
public:
    explicit Worker(TaskQueue& queue);
    ~Worker();

    // 注入完成回调（运行时回调，非编译期类型依赖）
    void set_completion_handler(std::function<void(const TaskResult&)> handler);

    void start();
    void join();
    void request_stop();

private:
    void loop();

    TaskQueue& queue_;
    std::function<void(const TaskResult&)> on_done_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
};

// Worker 池：拥有 N 个 Worker，统一注入完成回调并管理生命周期
class WorkerPool final {
public:
    WorkerPool(TaskQueue& queue, std::size_t worker_count);
    ~WorkerPool();

    void set_completion_handler(std::function<void(const TaskResult&)> handler);
    void start();
    void join();
    void request_stop();

private:
    std::vector<std::unique_ptr<Worker>> workers_;
};
