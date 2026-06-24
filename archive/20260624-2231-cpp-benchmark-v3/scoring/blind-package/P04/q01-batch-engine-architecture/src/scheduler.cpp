#include "scheduler.hpp"
#include "notifier.hpp"

#include <chrono>
#include <iostream>
#include <thread>

Scheduler::Scheduler(std::shared_ptr<ITaskStore> store,
                     std::unique_ptr<IRetryPolicy> retry,
                     std::shared_ptr<INotifier> notifier,
                     std::shared_ptr<MetricsCollector> metrics,
                     IClock& clock,
                     EngineConfig config)
    : store_(std::move(store)),
      retry_(std::move(retry)),
      notifier_(std::move(notifier)),
      metrics_(std::move(metrics)),
      clock_(clock),
      config_(std::move(config)),
      pool_(queue_, config_.worker_count) {}

TaskId Scheduler::submit(Task task) {
    task.id = TaskId{next_id_.fetch_add(1)};
    task.created_tick = clock_.now_ms();
    task.status = TaskStatus::Queued;
    task.attempt = 0;
    store_->save(task);
    queue_.push(task);
    metrics_->record_submit();
    return task.id;
}

std::size_t Scheduler::submitBatch(std::vector<Task> tasks) {
    std::size_t n = 0;
    for (auto& task : tasks) {
        submit(std::move(task));
        ++n;
    }
    return n;
}

void Scheduler::start() {
    if (running_.exchange(true)) {
        return;
    }
    // 运行时把完成回调注入 worker 池——这是 Worker->Scheduler 的运行时回调，
    // 并非编译期依赖（worker.hpp 不 include scheduler.hpp）
    pool_.set_completion_handler(
        [this](const TaskResult& result) { handle_completion(result); });
    pool_.start();
}

void Scheduler::run() {
    start();
    // 主调度循环：等待停止信号
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (queue_.size() == 0) {
            // 队列空且仍在运行，短暂让出
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    pool_.join();
}

void Scheduler::run_legacy() {
    // 兼容旧版调度循环：历史上先提交后启动两段式，现已合并进 run()。
    // 保留入口以兼容旧调用方，内部直接委托。
    run();
}

void Scheduler::stop() {
    running_.store(false);
    queue_.shutdown();
    pool_.request_stop();
}

void Scheduler::request_workers_stop() {
    pool_.request_stop();
}

void Scheduler::handle_completion(const TaskResult& result) {
    store_->update_status(result.id, result.ok ? TaskStatus::Succeeded : TaskStatus::Failed);

    if (result.ok) {
        metrics_->record_success();
        notifier_->on_event("task.succeeded", result);
        store_->remove(result.id);
        return;
    }

    // 失败：先看是否还有重试机会（result.attempt 是本次尝试序号，从 1 起）
    if (result.attempt >= config_.max_attempts ||
        (retry_ && !retry_->next_delay_ms(result.attempt).has_value())) {
        store_->update_status(result.id, TaskStatus::Failed);
        metrics_->record_failure();
        notifier_->on_event("task.failed", result);
        return;
    }

    // 还有重试机会：把已尝试次数同步回存储后重新入队，等待下次执行
    metrics_->record_retry();
    Task task;
    if (store_->load(result.id, task)) {
        task.attempt = result.attempt;  // worker 在本地累加的次数需同步回存储，否则永不递增
        task.status = TaskStatus::Retrying;
        store_->save(task);
        queue_.push(task);
    }
}

void Scheduler::reschedule(Task task) {
    task.status = TaskStatus::Queued;
    store_->save(task);
    queue_.push(std::move(task));
}
