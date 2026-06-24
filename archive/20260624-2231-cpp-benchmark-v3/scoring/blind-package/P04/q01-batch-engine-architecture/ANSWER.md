# 作答说明

## 架构推理概述

TaskForge 是一个进程内 C++17 批处理引擎，采用经典的依赖注入 + 组合根模式。`main.cpp` 是组合根，装配所有具体实现（InMemoryTaskStore、ExponentialBackoffPolicy、CompositeNotifier 等）并注入到 `Scheduler`。`Scheduler` 是编排核心，持有 `TaskQueue`（优先级队列）和 `WorkerPool`（N 个 Worker 线程），通过接口指针（ITaskStore、IRetryPolicy、INotifier、IClock）与基础设施解耦。Worker 从共享队列取任务执行，通过运行时注入的回调将 TaskResult 回传给 Scheduler 进行结算（成功/失败/重试）。

## 各视角的关键节点 / 边与源码依据

### 01 分层架构图

分为 5 层（含死代码层），依据编译期依赖方向：

1. **应用入口层**：`main.cpp` — 组合根，装配所有具体实现（main.cpp:21-39）
2. **编排层**：`Scheduler` — 持有所有接口指针和核心组件（scheduler.hpp:19-58）
3. **领域服务层**：`WorkerPool`、`Worker`、`TaskQueue`、`MetricsCollector`、`IRetryPolicy`、`Task` — 被 Scheduler 直接持有的执行与策略组件
4. **基础设施层**：`ITaskStore`、`InMemoryTaskStore`、`PersistentTaskStore`、`IClock`、`SystemClock`、`INotifier`、`ConsoleNotifier`、`CallbackNotifier`、`CompositeNotifier`、`EngineConfig` — 叶子节点，无向上依赖
5. **死代码层**：`TaskQueueLegacy`、`ILogger`、`ConsoleLogger`、`run_legacy()` — 编译进产物但无人调用

分层依据：Scheduler 的 `#include` 方向是从编排层向下依赖领域服务和基础设施，而基础设施层内部无任何向上 include。

### 02 组件依赖图

编译期依赖通过 `#include` 分析：
- `scheduler.hpp` include 了 task.hpp、task_queue.hpp、task_store.hpp、retry_policy.hpp、metrics.hpp、clock.hpp、worker.hpp、config.hpp（scheduler.hpp:2-10）
- `main.cpp` include 了 scheduler.hpp 和 notifier.hpp（main.cpp:1-2）
- `worker.hpp` 仅 include task.hpp（worker.hpp:2）— **不** include scheduler.hpp
- `task_queue.hpp` include task.hpp（task_queue.hpp:3）

**运行时回调**：Worker 通过 `set_completion_handler` 注入 `std::function<void(const TaskResult&)>` 回调（worker.hpp:21），由 Scheduler 在 `start()` 时注入（scheduler.cpp:48-49）。这是运行时 callback，不是编译期依赖 — worker.hpp 不 include scheduler.hpp。在图中用虚线标注为 `callback` 类型。

### 03 数据流图

完整数据流（对应 scheduler.cpp 和 worker.cpp）：
1. Client → Scheduler.submit()：分配 ID、记录 created_tick、状态设为 Queued（scheduler.cpp:22-31）
2. Scheduler → ITaskStore.save()：持久化任务（scheduler.cpp:27）
3. Scheduler → TaskQueue.push()：入队优先级队列（scheduler.cpp:28）
4. TaskQueue → Worker.blocking_pop()：Worker 线程短超时轮询（worker.cpp:38）
5. Worker → task.work()：执行业务负载（worker.cpp:51）
6. task.work() → TaskResult：产出结果（ok/code/attempt）（worker.cpp:43-44）
7. Worker → Scheduler.handle_completion()：通过运行时回调传回 TaskResult（worker.cpp:69-71）
8. handle_completion 分支：
   - 成功：ITaskStore.update_status(Succeeded) + MetricsCollector.record_success() + INotifier.on_event("task.succeeded") + ITaskStore.remove()（scheduler.cpp:85-89）
   - 失败且重试耗尽：ITaskStore.update_status(Failed) + record_failure() + on_event("task.failed")（scheduler.cpp:93-98）
   - 失败但可重试：record_retry() + ITaskStore.load() + 更新 attempt + save + queue.push()（scheduler.cpp:101-109）— 形成反馈环

### 04 类与模块关系图

**接口与实现**：
- ITaskStore ← InMemoryTaskStore, PersistentTaskStore（task_store.hpp:11-49）
- IRetryPolicy ← FixedBackoffPolicy, ExponentialBackoffPolicy（retry_policy.hpp:9-39）
- INotifier ← ConsoleNotifier, CallbackNotifier, CompositeNotifier（notifier.hpp:11-42）
- IClock ← SystemClock（clock.hpp:7-22）
- ILogger ← ConsoleLogger（logger.hpp:7-19）— 未使用

**组合**：Scheduler composes TaskQueue + WorkerPool（scheduler.hpp:55-56）；WorkerPool composes Worker（worker.hpp:48）

**聚合**：CompositeNotifier aggregates INotifier children（notifier.hpp:41）

**使用（依赖接口）**：Scheduler 持有 shared_ptr<ITaskStore>、unique_ptr<IRetryPolicy>、shared_ptr<INotifier>、IClock&（scheduler.hpp:48-51）— 依赖边画到接口而非具体实现

**组合根装配**（main.cpp:21-39）：InMemoryTaskStore、ExponentialBackoffPolicy、MetricsCollector、CompositeNotifier（含 ConsoleNotifier + CallbackNotifier）、SystemClock

### 05 部署 / 运行时拓扑图

运行时实例：
- **主线程**（×1）：运行 scheduler.run() 循环（scheduler.cpp:53-64）
- **Worker 线程**（×4）：由 EngineConfig.worker_count=4 决定（config.hpp:9, config.cpp:14）
- **停止线程**（×1）：demo 中 800ms 后调用 stop()（main.cpp:73-76）
- **共享资源**：TaskQueue 实例（×1，mutex + condvar 保护）、IRetryPolicy 实例（×1，ExponentialBackoff）、INotifier 实例（×1，CompositeNotifier）、IClock 实例（×1，SystemClock）

**编译进但未实例化**：PersistentTaskStore（main 只装配 InMemoryTaskStore）、TaskQueueLegacy（全仓库无人调用）、FixedBackoffPolicy（main 只装配 ExponentialBackoffPolicy）、ILogger/ConsoleLogger（全仓库无人调用）

## 陷阱处理（各举一例并指出源码证据）

### 命名相近的活跃类型 vs 死代码旧类型
- **TaskQueue**（task_queue.hpp:13-39）是活跃的优先级队列，Scheduler 和 Worker 都使用它
- **TaskQueueLegacy**（task_queue.hpp:42-52）签名相似、注释自称"通用任务队列，引擎各处使用"，但全仓库 grep 无一处调用 — 是死代码
- 处理方式：在分层图和部署图中将 TaskQueueLegacy 标注为灰色/死代码层

### 死代码 / 历史遗留
- **ILogger / ConsoleLogger**（logger.hpp）：定义了完整的日志抽象和实现，但 scheduler.hpp/cpp 和 main.cpp 均未 include logger.hpp，无任何代码路径使用
- **run_legacy()**（scheduler.hpp:37, scheduler.cpp:66-70）：保留的兼容入口，内部直接委托给 run()
- **PersistentTaskStore**（task_store.hpp:36-49）：编译进产物，但 main.cpp 只装配 InMemoryTaskStore
- 处理方式：在分层图单独一层标注、在部署图标注为"未实例化"

### 运行时回调 vs 编译期循环依赖
- Worker 通过 `set_completion_handler(std::function<void(const TaskResult&)>)` 注入回调（worker.hpp:21）
- Scheduler 在 `start()` 中注入 lambda `[this](const TaskResult& result) { handle_completion(result); }`（scheduler.cpp:48-49）
- 关键证据：worker.hpp **不** include scheduler.hpp（worker.hpp:1-3 仅 include task.hpp），因此不存在编译期循环依赖
- 处理方式：在组件依赖图中用虚线 + `data-kind="callback"` 区分，实线仅画编译期 `#include` 依赖

### 注释自称与真实分层不符
- **MetricsCollector**（metrics.hpp:8）注释自称"核心调度组件：引擎核心调度与统计"，但按依赖方向分析：它不依赖任何其他引擎组件（metrics.hpp 仅 include 标准库），是被 Scheduler 持有的叶子节点，属于领域服务层/基础设施层
- 处理方式：按真实依赖方向将其归入领域服务层，而非编排层

### 接口依赖 vs 具体类依赖
- Scheduler 持有 `shared_ptr<ITaskStore>`（scheduler.hpp:48）、`unique_ptr<IRetryPolicy>`（scheduler.hpp:49）、`shared_ptr<INotifier>`（scheduler.hpp:50）、`IClock&`（scheduler.hpp:51）— 全部是接口/抽象
- scheduler.hpp 对 INotifier 仅用前置声明（scheduler.hpp:15），不 include notifier.hpp
- 组合根 main.cpp 装配具体实现（InMemoryTaskStore、ExponentialBackoffPolicy 等）
- 处理方式：类关系图中 Scheduler 的 uses 边画到接口（ITaskStore、IRetryPolicy 等），不画到具体实现

## 验证

- **引擎 demo 运行**：编译通过（g++ -std=c++17），demo 成功运行，输出 `submitted=5 succeeded=4 failed=1 retried=4`，符合预期（1 个 flaky 任务重试后成功、1 个 always-fail 任务重试耗尽后失败、3 个 batch 任务成功）
- **SVG 检查**：`python3 svg_check.py` 全部通过，5 张图均满足 well-formed XML、data-style 合法、节点 ≥6、边 ≥4
- **SVG 生成方式**：用 Python 脚本（gen_svgs.py）生成，可完全复现
- **架构事实验证**：通过逐文件阅读 include 关系确认编译期依赖，通过 main.cpp 确认运行时装配，通过 grep 确认 TaskQueueLegacy/ILogger 无调用方

## 剩余风险

- PersistentTaskStore 虽编译进产物但未在 demo 中装配，部署图中标注为"未实例化"是基于 main.cpp 的判断；若存在其他入口点（如测试）可能装配它，但当前代码库中未发现
- FixedBackoffPolicy 同理，main.cpp 只装配了 ExponentialBackoffPolicy
- CompositeNotifier 的 aggregates 边指向 INotifier 接口（而非具体子通知器），因为 children_ 的类型是 `vector<shared_ptr<INotifier>>`（notifier.hpp:41）
