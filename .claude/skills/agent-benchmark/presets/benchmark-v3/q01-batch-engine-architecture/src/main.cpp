#include "scheduler.hpp"
#include "notifier.hpp"
#include "metrics.hpp"
#include "clock.hpp"
#include "config.hpp"
#include "logger.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main() {
    std::ios::sync_with_stdio(false);
    std::cout << "TaskForge demo starting\n" << std::flush;

    // ---- 组合根：在此装配所有具体实现 ----
    auto store = std::make_shared<InMemoryTaskStore>();
    auto retry = std::make_unique<ExponentialBackoffPolicy>(50, 2000, 3);
    auto metrics = std::make_shared<MetricsCollector>();

    auto composite = std::make_shared<CompositeNotifier>();
    composite->add(std::make_shared<ConsoleNotifier>());

    auto callback_notifier = std::make_shared<CallbackNotifier>(
        [](const std::string& event, const TaskResult& result) {
            if (event == "task.failed") {
                std::cout << "[cb] task " << result.id.value << " gave up after retries\n";
            }
        });
    composite->add(callback_notifier);

    SystemClock clock;
    EngineConfig config = ConfigLoader::default_for("default");

    Scheduler scheduler(store, std::move(retry), composite, metrics, clock, config);

    // 一个“前 N 次失败、之后成功”的任务工厂
    auto make_flaky = [](std::shared_ptr<std::atomic<int>> counter, int fail_first) {
        return [counter, fail_first]() -> int {
            if (counter->fetch_add(1) < fail_first) {
                return 1;  // 非 0 视为失败
            }
            return 0;
        };
    };

    // ---- 提交任务：submit 与 submitBatch 两个入口都演示 ----
    auto flaky1 = std::make_shared<std::atomic<int>>(0);
    scheduler.submit(Task{
        TaskId{}, "flaky-retry", Priority::High, 3,
        make_flaky(flaky1, 2), 0, TaskStatus::Pending, 0, 0});

    auto always_fail = std::make_shared<std::atomic<int>>(0);
    scheduler.submit(Task{
        TaskId{}, "always-fail", Priority::Normal, 2,
        make_flaky(always_fail, 999), 0, TaskStatus::Pending, 0, 0});

    std::vector<Task> batch;
    batch.reserve(3);
    for (int i = 0; i < 3; ++i) {
        batch.push_back(Task{
            TaskId{}, "batch-" + std::to_string(i), Priority::Low, 1,
            [i] { std::cout << "[work] batch-" << i << " done\n"; return 0; },
            0, TaskStatus::Pending, 0, 0});
    }
    scheduler.submitBatch(std::move(batch));

    // 在独立线程里驱动停止：等待任务大致处理完毕后停止引擎
    std::thread stopper([&scheduler] {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        scheduler.stop();
    });

    // 主调度循环：阻塞直至停止
    scheduler.run();

    if (stopper.joinable()) {
        stopper.join();
    }

    std::cout << "TaskForge demo finished\n";
    std::cout << "[summary] " << metrics->summary() << "\n" << std::flush;
    return 0;
}
