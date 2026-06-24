# 作答说明

## 架构推理概述

TaskForge 是一个单进程、多线程的 C++17 批处理引擎。其组合根在 `src/main.cpp`（约 L20-38）装配所有具体实现（`InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier` 含 `ConsoleNotifier+CallbackNotifier`、`SystemClock`、`MetricsCollector`），并注入到 `Scheduler`。`Scheduler` 是编排核心，通过接口指针持有各契约，自身不直接依赖任何具体实现；它通过 `TaskQueue`（线程安全 priority queue）入队任务，由 `WorkerPool`（4 个 Worker 线程）出队执行，Worker 通过运行时注入的 `std::function` 回调将 `TaskResult` 回传给 `Scheduler::handle_completion`，后者负责判断是否重试（通过 `IRetryPolicy`）、通知终态（通过 `INotifier`）、记录统计（通过 `MetricsCollector`）。

## 各视角的关键节点 / 边与源码依据

### 01 分层架构图

按真实编译期依赖方向分四层：

- **Layer A — 应用入口**：`main()`。依据：`main.cpp` L1 `#include "scheduler.hpp"` 及 `notifier.hpp` 等，main 处于所有模块的"最上层"。
- **Layer B — 编排层**：`Scheduler`、`WorkerPool`、`Worker`。依据：`scheduler.hpp` L1-10 包含 task/task_store/task_queue/retry_policy/metrics/clock/worker/config 8个头文件，是依赖扇出最大的模块，位于编排层；Worker 由 Scheduler 持有。
- **Layer C — 领域契约层**：`ITaskStore`、`IRetryPolicy`、`INotifier`、`IClock`、`Task`/`TaskResult`、`MetricsCollector`。依据：这些都是被 Scheduler 依赖的接口/领域类型，本身不依赖 scheduler。
- **Layer D — 基础设施层**：各具体实现（`InMemoryTaskStore`、`ExponentialBackoffPolicy`、`ConsoleNotifier` 等）、`TaskQueue`、`EngineConfig`/`ConfigLoader`。依据：它们实现了 Layer C 的接口，或为 Scheduler 提供底层服务。

Dead Code 单列一行：`TaskQueueLegacy`（`task_queue.hpp` L42-52，全仓库无人调用）、`PersistentTaskStore`（`task_store.hpp` L36-49，main 未装配）、`ILogger`/`ConsoleLogger`（`logger.hpp`，Scheduler 未使用）。

### 02 组件依赖图

**编译期依赖**：从 `#include` 关系直接推导。`scheduler.hpp` 包含 8 个头文件（`task.hpp`、`task_store.hpp`、`task_queue.hpp`、`retry_policy.hpp`、`metrics.hpp`、`clock.hpp`、`worker.hpp`、`config.hpp`），这是 Scheduler 的全部编译期依赖。`worker.hpp` 只包含 `task.hpp` 和 `task_queue.hpp`——它不包含 `scheduler.hpp`，证明 Worker→Scheduler 不是编译期依赖。

**运行时回调**：`Worker→Scheduler` 的通信路径是运行时回调。证据：`worker.hpp` L21 `void set_completion_handler(std::function<void(const TaskResult&)>)`；`scheduler.cpp` L48-49 `pool_.set_completion_handler([this](...) { handle_completion(...); })`。这条边在图中用虚线标注 `data-kind="callback"`。

### 03 数据流图

一次 Task 完整流转路径：
1. `Caller` 调用 `Scheduler::submit()`（`scheduler.cpp` L22-31）或 `submitBatch()`（L33-40）
2. Scheduler 给 Task 分配 `TaskId`、设 `status=Queued`、调 `store_->save()`、`queue_.push()`、`metrics_->record_submit()`
3. `Worker::loop()` 调 `queue_.blocking_pop()`（`worker.cpp` L38）取出 Task
4. Worker 执行 `task.work()`（L51），包装为 `TaskResult`（L42-46）
5. Worker 通过 `on_done_` 回调（L69-71）传给 `Scheduler::handle_completion()`（`scheduler.cpp` L82-110）
6. **成功分支**：`metrics_->record_success()`、`notifier_->on_event("task.succeeded", ...)`、`store_->remove()`
7. **重试分支**（L101-109）：如果 `retry_->next_delay_ms(attempt)` 有值，从 store 加载 Task、更新 attempt、重新 `queue_.push()`——这构成**重试反馈环**
8. **失败终态**（L93-98）：超过重试上限时 `metrics_->record_failure()`、`notifier_->on_event("task.failed", ...)`

### 04 类与模块关系图

**接口实现关系**（`implements`）：
- `InMemoryTaskStore` / `PersistentTaskStore` implements `ITaskStore`（`task_store.hpp` L22 / L36）
- `ExponentialBackoffPolicy` / `FixedBackoffPolicy` implements `IRetryPolicy`（`retry_policy.hpp` L17 / L29）
- `ConsoleNotifier` / `CallbackNotifier` / `CompositeNotifier` implements `INotifier`（`notifier.hpp` L18 / L24 / L35）
- `SystemClock` implements `IClock`（`clock.hpp` L14）

**聚合关系**（Scheduler 持有接口指针）：
- `Scheduler` aggregates `ITaskStore*`、`IRetryPolicy*`、`INotifier*`、`IClock&`、`MetricsCollector*`（`scheduler.hpp` L48-53）
- 注意：Scheduler 依赖的是**接口**（`ITaskStore`），而非具体实现（`InMemoryTaskStore`）

**合成关系**：
- `WorkerPool` composes `vector<unique_ptr<Worker>>`（`worker.hpp` L48）
- `CompositeNotifier` composes `vector<shared_ptr<INotifier>>`（`notifier.hpp` L41）
- `Worker` 引用 `TaskQueue&`（`worker.hpp` L30）

**组合根装配**（`main.cpp` L21-38）：
- `InMemoryTaskStore`（非 `PersistentTaskStore`）
- `ExponentialBackoffPolicy(50, 2000, 3)`（非 `FixedBackoffPolicy`）
- `CompositeNotifier` 含 `ConsoleNotifier` + `CallbackNotifier`
- `SystemClock`
- `MetricsCollector`

### 05 部署 / 运行时拓扑图

- **单进程**：main() 是入口
- **主线程**：运行 `Scheduler::run()`——sleep 20ms 循环（`scheduler.cpp` L53-64），本身不做任务执行
- **4 个 Worker 线程**：`WorkerPool(TaskQueue&, config.worker_count)` 创建（`scheduler.hpp` L20，`worker.cpp` L79-83），默认 `worker_count=4`（`config.cpp` L14）
- **共享汇合点**：唯一的 `TaskQueue` 实例（`scheduler.hpp` L55），Scheduler 入队、所有 Worker 出队
- **运行时注入**：`Scheduler::start()` 注入完成回调到 WorkerPool（`scheduler.cpp` L48-49），这是运行时行为
- **未实例化**：`TaskQueueLegacy`（无人调用）、`PersistentTaskStore`（main 未装配）、`ILogger`/`ConsoleLogger`（Scheduler 未使用）、`FixedBackoffPolicy`（main 未装配）

## 陷阱处理（各举一例并指出源码证据）

### 陷阱 1：命名相近但语义不同的类型（活跃类型 vs 死代码旧类型）

**处理**：在分层图、部署图中将 `TaskQueueLegacy` 标注为"Dead Code / 全仓库无人调用"，未将其放入核心架构路径。

**源码证据**：`task_queue.hpp` L42-52 定义了 `TaskQueueLegacy`（注释自称"通用任务队列，引擎各处使用"），但搜索全部 `src/*.cpp` 和 `include/*.hpp` 文件，`Scheduler` 使用的是 `TaskQueue`（priority queue, `scheduler.hpp` L55），`Worker` 持有 `TaskQueue&`（`worker.hpp` L17/30），无任何文件实例化或调用 `TaskQueueLegacy`。

### 陷阱 2：死代码 / 历史遗留

**处理**：在分层图中专设"Dead Code"行、在部署图中标注"未实例化"清单。

**源码证据**：
- `TaskQueueLegacy`（`task_queue.hpp` L42-52，`task_queue.cpp` L52-71）——编译完整但无调用方
- `PersistentTaskStore`（`task_store.hpp` L36-49）——注释说"已编译进产物，但组合根（main）默认装配 InMemoryTaskStore"，`main.cpp` L21 只实例化 `InMemoryTaskStore`
- `ILogger`/`ConsoleLogger`（`logger.hpp` L7-18）——`scheduler.hpp` 未包含 `logger.hpp`，Scheduler 未使用
- `Scheduler::run_legacy()`（`scheduler.cpp` L66-70）——注释"兼容旧版调度循环"，内部直接委托 `run()`
- `FixedBackoffPolicy`——`main.cpp` L22 只装配 `ExponentialBackoffPolicy`

### 陷阱 3：运行时回调 vs 编译期依赖

**处理**：在组件依赖图中，`Worker→Scheduler` 用虚线标 `data-kind="callback"` 而非常规实线 `depends`，明确区分。

**源码证据**：`worker.hpp` 不包含 `scheduler.hpp`（仅包含 `task.hpp` + `task_queue.hpp`），Worker 通过 `std::function<void(const TaskResult&)> on_done_`（L31）回报，该回调在 `scheduler.cpp` L48-49 运行时注入：`pool_.set_completion_handler([this](const TaskResult& result) { handle_completion(result); })`。这不是编译期依赖，不存在循环 #include。

### 陷阱 4：注释自称与真实分层不符

**处理**：将 `MetricsCollector` 放在基础设施层（Layer D），而非"核心调度层"。

**源码证据**：`metrics.hpp` L8 注释自称`"核心调度组件：引擎核心调度与统计"`，但其实际功能只是原子计数器统计（`record_submit/success/failure/retry`），不包含任何调度逻辑。按依赖方向看：Scheduler 依赖 MetricsCollector（`scheduler.hpp` L7 `#include "metrics.hpp"`），而 MetricsCollector 不依赖 Scheduler，按依赖方向属于底层基础设施。

### 陷阱 5：接口依赖 vs 具体类依赖

**处理**：在类关系图中，Scheduler 的聚合边（`data-kind="aggregates"`）指向**接口**（`ITaskStore`、`IRetryPolicy`、`INotifier`、`IClock`），而非具体实现（`InMemoryTaskStore`、`ExponentialBackoffPolicy` 等）。具体实现与接口的连接通过 `data-kind="implements"` 表达，由组合根（main）注入。

**源码证据**：
- `scheduler.hpp` L21 `std::shared_ptr<ITaskStore> store`——参数类型是 `ITaskStore`
- `scheduler.hpp` L22 `std::unique_ptr<IRetryPolicy> retry`——参数类型是 `IRetryPolicy`
- `scheduler.hpp` L23 `std::shared_ptr<INotifier> notifier`——参数类型是 `INotifier`
- `scheduler.hpp` L25 `IClock& clock`——参数类型是 `IClock`
- 具体实现由 `main.cpp` L21-37 装配并注入

**唯一例外**：`MetricsCollector`。Scheduler 直接持有 `std::shared_ptr<MetricsCollector>`（`scheduler.hpp` L24/L51），依赖的是**具体类**而非接口。这是设计中一个特例，在图中如实保留。

## 验证

### 生成方式

使用 Python 脚本 `generate_svgs.py` 生成所有 5 张 SVG。脚本内嵌 `esc()` 函数处理 XML 实体转义（`&`→`&amp;`），确保 XML well-formedness。可复现：运行 `python3 generate_svgs.py` 即可重新生成。

### `svg_check.py` 结果

```
[OK] 5 张约定命名的 SVG 齐全
[OK] 01_layered_architecture.svg: 标注合规 data-style=layered，节点 22(>=6)，边 16(>=4)
[OK] 02_component_dependency.svg: 标注合规 data-style=component，节点 14(>=6)，边 17(>=4)
[OK] 03_dataflow.svg: 标注合规 data-style=dataflow，节点 16(>=6)，边 16(>=4)
[OK] 04_class_relationship.svg: 标注合规 data-style=class，节点 22(>=6)，边 20(>=4)
[OK] 05_deployment_topology.svg: 标注合规 data-style=deployment，节点 14(>=6)，边 12(>=4)
公开 SVG 检查全部通过。
```

### `run_public_checks.sh` 结果

脚本分两步：① 编译并运行 demo（打印 `TaskForge demo starting` / `submitted=X succeeded=Y failed=Z retried=W` / `TaskForge demo finished`），证明源码可编译运行、运行时统计与架构预期一致；② 校验 SVG，全部通过。

## 剩余风险

- `Scheduler` 依赖 `MetricsCollector` 具体类（非接口），这是代码中的设计选择，图中如实表达，但若 ground truth 期望统一画为接口依赖则可能有偏差。
- 数据流图中"重试反馈环"的标注用 `data-kind="flow"`，未单独定义 `retry` kind（题面允许在 `dataflow` 用 `flow`），如果 ground truth 对 kind 有更细粒度要求可能有差异。
- `Scheduler::reschedule()` 方法（`scheduler.cpp` L112-116）在当前调用链中未被 `handle_completion` 使用（handle_completion 直接 load+push），但作为 public 入口保留在图中。
