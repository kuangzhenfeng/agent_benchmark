#pragma once

#include "task.hpp"
#include "task_store.hpp"
#include "task_queue.hpp"
#include "retry_policy.hpp"
#include "metrics.hpp"
#include "clock.hpp"
#include "worker.hpp"
#include "config.hpp"

#include <atomic>
#include <memory>

class INotifier;   // 前置声明：scheduler.hpp 不需要知道通知器的具体类型

// 调度器：批处理引擎的编排核心。通过依赖注入持有各契约的抽象，
// 自身只持有接口指针，不依赖任何具体实现。
class Scheduler {
public:
    Scheduler(std::shared_ptr<ITaskStore> store,
              std::unique_ptr<IRetryPolicy> retry,
              std::shared_ptr<INotifier> notifier,
              std::shared_ptr<MetricsCollector> metrics,
              IClock& clock,
              EngineConfig config);

    // 提交单个任务（公开入口之一）
    TaskId submit(Task task);
    // 批量提交任务（公开入口之二）
    std::size_t submitBatch(std::vector<Task> tasks);

    void start();
    // 主调度循环：由组合根（main）调用，阻塞直至队列停止
    void run();
    // 兼容旧版调度循环（历史遗留入口）
    void run_legacy();
    void stop();

    // 供测试/组合根：请求 worker 停止
    void request_workers_stop();

private:
    // 任务完成回调：由 Worker 在运行时注入后回调用
    void handle_completion(const TaskResult& result);
    void reschedule(Task task);

    std::shared_ptr<ITaskStore> store_;
    std::unique_ptr<IRetryPolicy> retry_;
    std::shared_ptr<INotifier> notifier_;
    std::shared_ptr<MetricsCollector> metrics_;
    IClock& clock_;
    EngineConfig config_;

    TaskQueue queue_;
    WorkerPool pool_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> next_id_{1};
};
