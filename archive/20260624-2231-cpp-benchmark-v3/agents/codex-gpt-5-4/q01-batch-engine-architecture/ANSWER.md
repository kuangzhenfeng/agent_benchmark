# 作答说明

## 架构推理概述

`TaskForge` 的组合根在 `src/main.cpp:20-39`。`main()` 负责装配 `InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`、`MetricsCollector`、`SystemClock` 和 `EngineConfig`，再把这些依赖注入 `Scheduler`。`Scheduler` 是编排核心：`submit()` 负责分配 `TaskId`、写存储、入 `TaskQueue`，`run()` / `start()` 负责启动 `WorkerPool`，`handle_completion()` 负责根据执行结果更新状态、计数、通知和重试（`src/scheduler.cpp:22-30`, `42-50`, `82-109`）。`Worker` 只从共享 `TaskQueue` 拉任务、执行 `Task::work`、生成 `TaskResult` 并通过运行时注入的回调把结果回传给 `Scheduler`（`src/worker.cpp:34-71`）。

## 各视角的关键节点 / 边与源码依据

### 01 分层架构图

- 顶层我画成 `main` / `ConfigLoader`，依据是组合根在 `src/main.cpp:20-39`，配置加载在 `src/config.cpp::ConfigLoader::default_for`，它们只向下装配，不被下层反向依赖。
- 第二层只有 `Scheduler`，因为真正的编排逻辑都集中在 `Scheduler::submit`、`Scheduler::run`、`Scheduler::handle_completion`（`src/scheduler.cpp`）。
- 第三层是契约和模型：`Task`、`ITaskStore`、`IRetryPolicy`、`INotifier`、`IClock`。证据分别在 `include/task.hpp`、`include/task_store.hpp`、`include/retry_policy.hpp`、`include/notifier.hpp`、`include/clock.hpp`。
- 最底层是叶子实现与并发基础设施：`TaskQueue`、`WorkerPool`、`InMemoryTaskStore`、`PersistentTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`、`SystemClock`、`MetricsCollector`。`Scheduler` 直接依赖它们或它们所在头文件，而这些类型不反向依赖 `Scheduler`（`include/scheduler.hpp:17-58`）。
- `MetricsCollector` 虽然注释写着“核心调度组件”（`include/metrics.hpp`），但真实依赖方向是 `Scheduler -> MetricsCollector`，它自己只提供原子计数和 `summary()`（`src/metrics.cpp`），所以在分层里应归到基础设施叶子而不是核心编排层。

### 02 组件依赖图

- `main -> Scheduler / InMemoryTaskStore / ExponentialBackoffPolicy / CompositeNotifier / SystemClock / MetricsCollector / ConfigLoader`，依据是这些具体类型都在 `main()` 里被直接构造或调用（`src/main.cpp:21-39`）。
- `Scheduler -> ITaskStore / IRetryPolicy / INotifier / IClock / MetricsCollector / TaskQueue / WorkerPool`，依据是构造函数签名和成员字段（`include/scheduler.hpp:21-58`）。
- `WorkerPool -> Worker -> TaskQueue` 是真实编译期依赖。`WorkerPool` 在构造里创建多个 `Worker`（`src/worker.cpp:79-83`），`Worker` 在 `loop()` 中调用 `queue_.blocking_pop()`（`src/worker.cpp:34-40`）。
- `Worker -> Scheduler` 我刻意画成 `callback` 而不是 `depends`。原因是 `Worker` 只持有 `std::function<void(const TaskResult&)> on_done_`（`include/worker.hpp:79-92`），`Scheduler::start()` 在运行时注入 lambda（`src/scheduler.cpp:42-50`），`worker.hpp` 并没有 `#include "scheduler.hpp"`。
- `InMemoryTaskStore`、`PersistentTaskStore` 依赖 `ITaskStore`；`ExponentialBackoffPolicy`、`FixedBackoffPolicy` 依赖 `IRetryPolicy`；`ConsoleNotifier`、`CallbackNotifier`、`CompositeNotifier` 依赖 `INotifier`；`SystemClock` 依赖 `IClock`。这些关系都来自对应头文件里的 `final : public Interface` 声明。

### 03 数据流图

- 入口是 `Task -> Scheduler`，对应 `submit()` / `submitBatch()`（`src/scheduler.cpp:22-39`）。
- `Scheduler` 在提交时会先设置 `TaskId`、`created_tick`、`Queued` 状态，再 `store_->save(task)`，然后 `queue_.push(task)`，最后 `metrics_->record_submit()`（`src/scheduler.cpp:22-30`）。
- `Worker` 从 `TaskQueue::blocking_pop()` 取任务后执行 `task.work()`，生成 `TaskResult`，并把 `attempt` 填成“本次尝试序号”（`src/worker.cpp:34-66`）。
- `TaskResult -> Scheduler` 的回流发生在 `on_done_(result)`（`src/worker.cpp:68-70`）和 `Scheduler::handle_completion()`（`src/scheduler.cpp:82-109`）。
- 成功路径：`Scheduler` 更新 `Succeeded`，记 success，发 `task.succeeded` 事件，并 `store_->remove(result.id)`（`src/scheduler.cpp:85-89`）。
- 失败路径分两段：
  - 若 `result.attempt >= config_.max_attempts`，或 `retry_->next_delay_ms(result.attempt)` 返回空，则终态为 `Failed`，记 failure，发 `task.failed`（`src/scheduler.cpp:92-98`）。
  - 否则进入重试反馈环：记 retry，`store_->load()` 原任务，同步 `task.attempt = result.attempt`，改为 `Retrying`，再次 `save()` 并 `queue_.push(task)`（`src/scheduler.cpp:101-109`）。
- 图里专门把 `TaskStatus::Retrying` 单独画成中间状态，是为了表达“失败不是直接回队列，而是先同步存储中的 attempt/status，再重新入队”。

### 04 类与模块关系图

- 接口实现关系都来自头文件声明：`InMemoryTaskStore` / `PersistentTaskStore` 实现 `ITaskStore`，`FixedBackoffPolicy` / `ExponentialBackoffPolicy` 实现 `IRetryPolicy`，`ConsoleNotifier` / `CallbackNotifier` / `CompositeNotifier` 实现 `INotifier`，`SystemClock` 实现 `IClock`。
- `Scheduler` 对 `ITaskStore`、`IRetryPolicy`、`INotifier`、`MetricsCollector` 用的是注入后保存的指针/引用（`include/scheduler.hpp:48-53`），所以我画成“聚合/使用抽象”，而不是依赖具体实现。
- `Scheduler` 对 `TaskQueue` 和 `WorkerPool` 是直接成员（`include/scheduler.hpp:55-56`），`WorkerPool` 对 `Worker` 是 `vector<unique_ptr<Worker>>`（`include/worker.hpp:95-107`），这两组关系画成组合。
- `CompositeNotifier` 持有 `vector<shared_ptr<INotifier>> children_`（`include/notifier.hpp:142-149`），所以我画成对 `INotifier` 的聚合，而不是对 `ConsoleNotifier` / `CallbackNotifier` 的静态依赖。
- `main` 被画成组合根，因为具体实现实例是在 `main()` 里被真正创建并传给 `Scheduler` 的（`src/main.cpp:21-39`）。

### 05 部署 / 运行时拓扑图

- 运行时是单进程。`run_public_checks.sh` 会把所有 `src/*.cpp` 编成一个 demo 可执行文件（`run_public_checks.sh`）。
- 默认档位来自 `ConfigLoader::default_for("default")`（`src/main.cpp:36-37`, `src/config.cpp`），对应 `worker_count = 4`、`max_attempts = 3`，所以拓扑图里画了 4 个 worker 线程。
- 主线程负责跑 `scheduler.run()`（`src/main.cpp:78-79`），另有一个 `stopper` 线程在 800ms 后调用 `scheduler.stop()`（`src/main.cpp:72-76`）。
- `WorkerPool` 在构造时创建 N 个 `Worker`（`src/worker.cpp:79-83`），多个 `Worker` 共享同一个 `TaskQueue&`（`include/worker.hpp:89`, `src/worker.cpp:9`, `79`）。
- 运行时真实被实例化的是 `InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`、`ConsoleNotifier`、`CallbackNotifier`、`MetricsCollector`、`SystemClock`、`Scheduler`、`WorkerPool`、`Worker`、`TaskQueue`。
- 图中灰色侧栏单独标了“已编译但未实例化”的类型：`PersistentTaskStore`、`TaskQueueLegacy`、`ConsoleLogger`、`FixedBackoffPolicy`。依据是它们都有完整定义，但 `main()` 没有创建这些类型实例，且 `TaskQueueLegacy` 在源码中没有活跃调用点。

## 陷阱处理（各举一例并指出源码证据）

- 命名相近的活跃类型 vs 死代码旧类型：
  - 活跃的是 `TaskQueue`，`Scheduler::submit()` 入队用 `queue_.push(task)`，`Worker::loop()` 出队用 `queue_.blocking_pop()`（`src/scheduler.cpp:27-28`, `src/worker.cpp:38`）。
  - `TaskQueueLegacy` 虽然注释写“引擎各处使用”（`include/task_queue.hpp`），但全局搜索只命中声明和定义，没有活跃调用者，所以我只在部署图里标成灰色遗留类型，不把它画进活跃数据流。
- 死代码 / 历史遗留：
  - `Scheduler::run_legacy()` 和 `Scheduler::reschedule()` 都有实现（`src/scheduler.cpp:66-70`, `112-116`），但主路径只调用 `run()` / `handle_completion()`。我没有把 `run_legacy()` 放进主执行图，只在文字里说明它是兼容入口。
- 运行时回调 vs 编译期循环依赖：
  - `Worker` 并不依赖 `Scheduler` 类型；它只接收一个 `std::function`（`include/worker.hpp:79-92`），`Scheduler::start()` 运行时注入 lambda（`src/scheduler.cpp:46-49`），`Worker::loop()` 通过 `on_done_(result)` 回调（`src/worker.cpp:68-70`）。所以组件图用 `callback`，不是双向 `depends`。
- 注释自称与真实分层不符：
  - `MetricsCollector` 的注释写“核心调度组件”（`include/metrics.hpp`），但真实上它既不调度线程也不控制状态流转，只是被 `Scheduler` 调用来累计计数（`src/scheduler.cpp:29`, `86`, `96`, `102`）。因此我把它放在基础设施叶子层。
- 接口依赖 vs 具体类依赖：
  - `Scheduler` 构造函数签名直接说明它依赖的是 `shared_ptr<ITaskStore>`、`unique_ptr<IRetryPolicy>`、`shared_ptr<INotifier>`、`IClock&`（`include/scheduler.hpp:21-26`）；而具体的 `InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`、`SystemClock` 是在 `main()` 里装配进去的（`src/main.cpp:21-39`）。因此类图和组件图都把 `Scheduler` 的边画向抽象接口，而不是具体实现。

## 验证

- 产物生成方式：5 张图均为手写 SVG，直接替换 `diagrams/*.svg` 占位文件；未引入外部绘图依赖，也未改动引擎源码。
- 自检命令：`./run_public_checks.sh`
- 结果：
  - demo 编译运行通过，实际输出摘要为 `submitted=5 succeeded=4 failed=1 retried=4`，并出现 4 次成功事件、1 次失败事件以及失败回调日志；
  - 输出顺序能反映并发消费：`batch-*` 三个低优先级任务的完成顺序并不固定，但 `flaky-retry` 最终成功、`always-fail` 最终失败，与 `Scheduler::handle_completion()` 的重试逻辑一致；
  - `svg_check.py` 5 张图全部通过，公开检查统计分别为：
    - `01_layered_architecture.svg`：17 节点，19 边
    - `02_component_dependency.svg`：21 节点，25 边
    - `03_dataflow.svg`：12 节点，14 边
    - `04_class_relationship.svg`：19 节点，25 边
    - `05_deployment_topology.svg`：20 节点，30 边

## 剩余风险

- `Scheduler` 的重试上限判断实际用的是 `config_.max_attempts` 和 `IRetryPolicy::next_delay_ms()`（`src/scheduler.cpp:92-95`），而不是 `Task.max_attempts` 字段；因此如果只看 `Task` 结构体，很容易误会“每个任务自己决定重试次数”。我在图和文字中按真实代码路径表达了这一点，但它仍是源码里最容易被忽略的行为细节之一。
- `retry_->next_delay_ms()` 目前只被当作“是否还能重试”的判定，返回的 delay 并没有真正用于延时调度；我在数据流图里把它画成 retry gate，而没有误画成定时器或延迟队列。
