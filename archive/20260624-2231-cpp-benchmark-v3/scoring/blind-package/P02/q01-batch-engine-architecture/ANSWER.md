# 作答说明

## 架构推理概述

`src/main.cpp` 是组合根：它创建具体存储、重试策略、通知器树、指标、时钟和配置，再把它们以抽象契约传给 `Scheduler`（20–39 行）。`Scheduler` 是单进程内的编排核心：提交时持久化并入 `TaskQueue`，启动 `WorkerPool`，再由完成回调结算结果、重试或通知终态。`Worker` 从同一个优先级队列消费任务，在各自线程执行 `Task::work`，仅通过运行时注入的函数回传 `TaskResult`。

五张图均为手写 SVG；每个节点与边都有 `data-*` 标注，箭头语义与图例一致。

## 各视角的关键节点 / 边与源码依据

### 01 分层架构图

- 应用层只有组合根 `main` 与配置装配；`main.cpp:21–39` 构造具体实现并传入 `Scheduler`。
- 编排层是 `Scheduler`。其头文件直接依赖队列、worker、任务、各抽象服务和 `EngineConfig`（`include/scheduler.hpp:3–10`），成员中自有 `TaskQueue` 与 `WorkerPool`（55–56 行）。
- 执行/领域层放置 `Task`、`TaskResult`、`TaskQueue` 和 `WorkerPool`。`TaskQueue` 的优先级比较器位于 `include/task_queue.hpp:24–35`；worker 消费和执行循环位于 `src/worker.cpp:34–71`。
- 存储、通知、重试、时钟、指标位于基础设施/可替换策略层。上层指向它们的接口，而实现再指向接口。`MetricsCollector` 虽有“核心调度”注释，却不依赖调度器，故按实际依赖是基础设施叶子。

### 02 组件依赖图

- `Scheduler` 的编译期依赖指向 `TaskQueue`、`WorkerPool`、`ITaskStore`、`IRetryPolicy`、`INotifier`、`MetricsCollector`、`IClock`、`Task` 和配置；具体边来自 `include/scheduler.hpp:3–10` 与 `src/scheduler.cpp:1–2`。
- `Worker` 模块依赖 `TaskQueue` 与 `Task`（`include/worker.hpp:3–4`），而 `TaskQueue` 和 `ITaskStore` 都依赖任务模型（各自头文件第 3 行）。
- 实线不把 `WorkerPool` 画成对 `Scheduler` 的编译期依赖。橙色虚线是 `Scheduler::start()` 注入 lambda（`src/scheduler.cpp:46–50`）后，经 `Worker::on_done_` 产生的运行时完成回调（`src/worker.cpp:68–71`）。

### 03 数据流图

1. 调用方经 `submit` / `submitBatch` 给出 `Task`；`submit` 分配 ID、写入 `ITaskStore`、设为 `Queued`、推入 `TaskQueue`、记录提交指标（`src/scheduler.cpp:22–40`）。
2. `Worker` 通过 `blocking_pop` 从共享优先级队列取任务，递增尝试次数并执行 `Task::work`；返回码或异常统一归为 `TaskResult`（`src/worker.cpp:34–70`）。
3. 回调进入 `handle_completion`，成功时更新状态、记录成功、发送 `task.succeeded` 并从 store 删除（`src/scheduler.cpp:82–89`）。
4. 失败时先更新为失败；若还能重试则读取任务、同步 `attempt`、存为 `Retrying` 后再次入队（92–109 行），形成图中的反馈环；否则记录失败并发送 `task.failed`。
- 图中特别注明了真实实现：`next_delay_ms()` 的返回值只以 `has_value()` 判断资格（93–94 行），随后直接 `queue_.push`（108 行），当前没有实际等待该 delay。

### 04 类与模块关系图

- `InMemoryTaskStore` / `PersistentTaskStore` 实现 `ITaskStore`，两个 backoff 策略实现 `IRetryPolicy`，`SystemClock` 实现 `IClock`，三种通知器实现 `INotifier`（相应声明见 `include/task_store.hpp:11–43`、`retry_policy.hpp:9–39`、`clock.hpp:7–21`、`notifier.hpp:11–41`）。
- `Scheduler` 组合拥有 `TaskQueue`、`WorkerPool`、`unique_ptr<IRetryPolicy>` 与值类型配置；对 store、notifier、metrics 为 `shared_ptr` 聚合，对 `IClock` 为引用使用（`include/scheduler.hpp:21–26,48–56`）。
- `WorkerPool` 拥有 `Worker`，每个 `Worker` 引用共享 `TaskQueue` 并使用 `TaskResult`（`include/worker.hpp:30–33,47–48`）。
- 组合根只装配 `InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`（含 console 和 callback 子通知器）与 `SystemClock`；因此 `Scheduler` 指向抽象而不是任何具体实现。

### 05 部署 / 运行时拓扑图

- 默认档位产生 `worker_count = 4`（`src/config.cpp:13–16`）。`Scheduler` 构造时以该数值构建 pool（`src/scheduler.cpp:14–20`），pool 在构造时创建 N 个 `Worker`（`src/worker.cpp:79–84`），`start()` 为每个 worker 创建一个 `std::thread`（99–102 行）。
- 运行时是一进程内的主线程、一个 stopper 线程和四个 worker 线程；主线程运行 `scheduler.run()`，stopper 在约 800ms 后调用 `stop()`（`src/main.cpp:72–82`）。
- 四个 worker 线程连接同一 `TaskQueue`，因此队列是共享的同步汇合点。store、metrics、通知器图同样是 scheduler 注入的共享对象。
- `PersistentTaskStore`、`FixedBackoffPolicy`、`TaskQueueLegacy`、`ConsoleLogger` 已编译但 demo 不实例化；图中以灰色虚线区与实际拓扑分开。

## 陷阱处理（各举一例并指出源码证据）

- **活跃队列与旧队列**：运行中的 `Scheduler` 成员是 `TaskQueue queue_`（`include/scheduler.hpp:55`），它使用 `std::priority_queue` 和优先级/FIFO 比较器（`include/task_queue.hpp:24–35`）。`TaskQueueLegacy` 仅有自己的 FIFO 实现（`src/task_queue.cpp:52–71`），没有被 scheduler 或 main 使用；因此只在部署图的“已编译未实例化”区标注，未画入核心数据流。
- **死代码 / 历史遗留**：`Scheduler::run_legacy()` 仅委托给 `run()`（`src/scheduler.cpp:66–70`），而 main 直接调用 `run()`（`src/main.cpp:79`）；`reschedule()` 也有定义但没有调用点（112–116 行）。图中没有把它们作为运行主链路。
- **运行时回调不是编译期循环**：`worker.hpp` 不包含 `scheduler.hpp`，只保存 `std::function<void(const TaskResult&amp;)>`（`include/worker.hpp:20–32`）；`Scheduler::start` 在运行时安装 lambda。因此组件图和部署图把回传标为 `callback` 虚线，而非 `depends` 实线。
- **注释自称不等于分层归属**：`MetricsCollector` 的注释称“核心调度组件”（`include/metrics.hpp:7`），但类本身只是原子计数和 `summary()`（8–26 行），不依赖 `Scheduler`。分层图据依赖方向把它放在基础设施叶子。
- **接口依赖不是具体依赖**：构造函数和成员明确使用 `ITaskStore`、`IRetryPolicy`、`INotifier` 和 `IClock`（`include/scheduler.hpp:21–26,48–52`）；具体的内存存储、指数策略与通知器只在 `main.cpp:21–39` 选择。类图和组件图因此将 Scheduler 的边连到接口。

## 验证

已运行 `./run_public_checks.sh`，结果通过：

- C++17 demo 编译和运行成功；本次输出汇总为 `submitted=5 succeeded=4 failed=1 retried=4`。终态事件包含 4 个成功和 1 个最终失败，符合任务工厂和重试逻辑。
- `svg_check.py` 检查全部通过：5 张约定文件均存在、XML 可解析、根为 SVG、拥有 `viewBox`，且 `data-style`、节点容器、边容器和最低节点/边数均合规。
- 图的公开检查节点/边计数依次为：分层 16/17、组件 17/24、数据流 10/13、类关系 20/25、部署 15/20。

## 剩余风险

图按 demo 的 `default` 配置表达四个 worker；若调用方改用 `fast` 或 `robust` profile，worker 数会分别变为 2 或 8，但拓扑结构不变。当前代码的退避延时未真正执行，图中已将其如实标注为“资格判断而非延迟调度”。
