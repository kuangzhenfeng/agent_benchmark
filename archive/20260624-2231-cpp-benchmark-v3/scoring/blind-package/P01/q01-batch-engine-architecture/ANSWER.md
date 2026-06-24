# 作答说明

## 架构推理概述

TaskForge 是一个**进程内**的批处理 / 任务调度引擎，采用经典的**依赖注入 + 组合根**架构：

- **组合根在 `main`（`src/main.cpp:16`）**：在这里 `make_shared`/`make_unique` 装配所有**具体实现**（`InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`、`SystemClock`、`MetricsCollector`），再把它们以**抽象接口指针**的形式注入 `Scheduler`。
- **`Scheduler`（`scheduler.hpp:19`）是编排核心**：自身只持有 `ITaskStore*` / `IRetryPolicy*` / `INotifier*` / `IClock&` 等接口指针，不依赖任何具体实现；它拥有值成员 `TaskQueue queue_` 和 `WorkerPool pool_`。
- **`TaskQueue`（`task_queue.hpp:13`）是共享汇合点**：`priority_queue` 大根堆（priority 高者先出，同优先级按 `created_tick` FIFO），线程安全；`Scheduler` 入队、`Worker` 出队。
- **`WorkerPool`（`worker.hpp:37`）拥有 N 个 `Worker`**（默认 `worker_count=4`，见 `config.cpp` 默认档位），每个 `Worker` 一个线程，从同一队列 `blocking_pop` 取任务执行，结果通过**运行时注入的回调** `on_done_` 回报给 `Scheduler::handle_completion` 结算。
- **`handle_completion`（`scheduler.cpp:82`）结算**：成功则记 `succeeded`、通知 `task.succeeded`、`remove`；失败且仍有重试机会则把 `attempt` 写回 store 后重新 `push`（重试反馈环）；耗尽重试则记 `failed`、通知 `task.failed`。

5 张图均由脚本 `gen_svgs.py`（本目录内）生成，坐标由脚本计算、端点自动落到节点矩形边缘，**可复现**（`python3 gen_svgs.py`）。

## 各视角的关键节点 / 边与源码依据

### 01 分层架构图（`data-style="layered"`）

按**真实 `#include` 与类型成员依赖方向**分 5 层（而非注释自称）：

- **L4 应用入口 / 组合根**：`main`（`src/main.cpp`）—— 仅装配具体实现。
- **L3 编排核心**：`Scheduler`（`scheduler.hpp:19`）。
- **L2 执行单元**：`WorkerPool`、`Worker`、`TaskQueue`（在用的优先级队列）。
- **L1 领域契约 / 汇合**：`ITaskStore`、`INotifier`、`IRetryPolicy`、`IClock`（全是纯抽象接口）。
- **L0 基础设施 / 值类型**：`Task`、`MetricsCollector`、`EngineConfig`。
- **侧栏（死代码）**：`TaskQueueLegacy`。

分层依据：`scheduler.hpp` 的 `#include` 清单（`task.hpp/task_store.hpp/task_queue.hpp/retry_policy.hpp/metrics.hpp/clock.hpp/worker.hpp/config.hpp`）——上层只 include 下层；`main.cpp` 仅 include 编排与具体实现。

### 02 组件依赖图（`data-style="component"`）

编译期依赖（`data-kind="depends"`）依据各头文件的 `#include` 与成员类型；运行时回调（`data-kind="callback"`）单独标注：
- `WorkerPool → Scheduler`（`callback`）：`Scheduler::start()` 运行时通过 `pool_.set_completion_handler([this](...){handle_completion(...);})` 注入（`scheduler.cpp:48`）。
- `Worker → Scheduler`（`callback`）：`Worker::loop` 末尾 `on_done_(result)`（`worker.cpp:69`）。

关键区分点见陷阱处理（运行时回调 vs 编译期循环依赖）。

### 03 数据流图（`data-style="dataflow"`）

一次任务的完整流转（`data-kind="flow"`），按 `submit → save → push → worker 执行 → TaskResult → handle_completion → 重试环 / 终态通知`：
1. `Client` 调 `submit`/`submitBatch`（`scheduler.cpp:22/33`）。
2. `Scheduler::submit`：分配 id、设 `Queued`、`store_->save`（`scheduler.cpp:27`）、`queue_.push`（`scheduler.cpp:28`）、`metrics_->record_submit`。
3. `Worker::loop`：`blocking_pop` 取出 → 置 `Running`、`attempt+1` → 执行 `task.work()` → 组装 `TaskResult`（`worker.cpp:34-71`）。
4. 通过回调把 `TaskResult` 回到 `Scheduler::handle_completion`（`scheduler.cpp:82`）。
5. 成功：`update_status(Succeeded)`、`record_success`、`on_event("task.succeeded")`、`remove`。
6. 失败仍有重试机会：`load` → 同步 `attempt` → `save(Retrying)` → `queue_.push`（`scheduler.cpp:101-109`，**重试反馈环**）。
7. 失败且耗尽重试：`update_status(Failed)`、`record_failure`、`on_event("task.failed")`。

### 04 类与模块关系图（`data-style="class"`）

- **implements**：8 个具体类实现各自接口（`InMemoryTaskStore/PersistentTaskStore → ITaskStore`；`Console/Callback/CompositeNotifier → INotifier`；`Fixed/ExponentialBackoffPolicy → IRetryPolicy`；`SystemClock → IClock`）。
- **composes / aggregates**：`Scheduler` 对存储/重试/通知的依赖一律画到**抽象接口**（`aggregates ITaskStore/IRetryPolicy/INotifier`、`uses IClock&`、`aggregates MetricsCollector`）；值成员 `queue_`/`pool_` 画为 `composes`。`WorkerPool composes Worker[unique_ptr]`、`Worker aggregates TaskQueue&`。
- **组合模式**：`CompositeNotifier aggregates INotifier`（`children_`，`notifier.hpp:35`）。
- **组合根装配**：`main composes InMemoryTaskStore / ExponentialBackoffPolicy / CompositeNotifier / SystemClock / MetricsCollector`（`main.cpp:21-39`）。

### 05 部署 / 运行时拓扑图（`data-style="deployment"`）

- **进程**：单一可执行（`run_public_checks.sh` 编译产物）。
- **线程**：`main thread`（跑 `Scheduler::run()`）、`stopper thread`（800ms 后 `stop()`，`main.cpp:73`）、`4 × worker threads`（`worker_count=4`，`config.cpp` 默认档位 + `EngineConfig::worker_count{4}`）。
- **实例与归属**：1 个 `Scheduler`（栈对象）→ `contains` 值成员 `TaskQueue`、`WorkerPool`，以及注入的 `InMemoryTaskStore`/`ExponentialBackoffPolicy`/`CompositeNotifier`/`MetricsCollector`/`SystemClock`；`WorkerPool contains 4×Worker`；`CompositeNotifier contains ConsoleNotifier + CallbackNotifier`。
- **共享汇合点**：4 个 `Worker` 都 `connects TaskQueue`（共享 `blocking_pop`）。
- **runs-on**：`Scheduler` 跑在 main 线程，`Worker#n` 跑在 worker 线程池。
- **callback**：`WorkerPool → Scheduler`（注入 completion handler）。
- **编译进产物但运行时未实例化**（侧栏红框）：`PersistentTaskStore`、`FixedBackoffPolicy`、`ConsoleLogger/ILogger`、`TaskQueueLegacy`。

## 陷阱处理（各举一例并指出源码证据）

1. **命名相近但语义不同的类型**：`TaskQueue`（`task_queue.hpp:13`，优先级 `priority_queue`，`Scheduler.queue_`/`Worker` 真正使用）vs `TaskQueueLegacy`（`task_queue.hpp:42`，注释自称「通用任务队列，引擎各处使用」，但**全仓库无人调用**）。处理：`TaskQueueLegacy` 不进入主流水线，标注为死代码侧栏（5 张图中均不上 depends/flow 边）。

2. **死代码 / 历史遗留**：
   - 类：`TaskQueueLegacy`（grep 全仓库仅自身定义，无调用点）；`PersistentTaskStore`、`FixedBackoffPolicy`（已编译进产物但 `main` 从未 `new`）；`ILogger/ConsoleLogger`（`src/` 中零引用，连 `.cpp` 都没有，仅头文件）。
   - 方法：`TaskQueue::try_pop`（定义于 `task_queue.cpp:11`，但 `Worker` 用的是 `blocking_pop`，无人调用 `try_pop`）、`Scheduler::reschedule`（`scheduler.cpp:112`，无人调用）、`Scheduler::run_legacy`（`scheduler.cpp:66`，仅委托 `run()`，main 未调用）、`Scheduler::request_workers_stop`（`scheduler.cpp:78`，无人调用）。
   - 处理：死代码不进入核心依赖/数据流边；部署图用红色侧栏集中标注「编译进产物但运行时未实例化」。

3. **运行时回调 vs 编译期循环依赖**：`Worker` 通过 `set_completion_handler` 注入的 `std::function`「反向」调用上层 `Scheduler::handle_completion`（`scheduler.cpp:48`、`worker.cpp:69`）。证据：`worker.hpp` **不 include** `scheduler.hpp`，`Worker` 只持有 `std::function<void(const TaskResult&)>`，不引用 `Scheduler` 类型。处理：组件图画为 `data-kind="callback"`（橙色虚线），**不**画成 `depends`，避免伪造成编译期循环依赖。

4. **注释自称与真实归属不符**：`MetricsCollector` 注释自称「核心调度组件：引擎核心调度与统计」（`metrics.hpp:7`），但按依赖方向它只被 `Scheduler`/`main` 使用、无任何下向依赖，实为**基础设施叶子节点**。处理：分层图中 `MetricsCollector` 归入 **L0 基础设施**，而非核心层。

5. **接口依赖 vs 具体类依赖**：`Scheduler` 成员是 `shared_ptr<ITaskStore>`/`unique_ptr<IRetryPolicy>`/`shared_ptr<INotifier>`/`IClock&`（`scheduler.hpp:48-52`），具体实现由 `main` 装配。处理：类关系图中 `Scheduler → ITaskStore/IRetryPolicy/INotifier` 一律画为**依赖抽象接口**的 `aggregates/uses` 边，**不**画到 `InMemoryTaskStore`/`ExponentialBackoffPolicy`；具体实现通过 `main composes ...` 的装配边体现。

## 验证

### 方式
- SVG 由脚本 `gen_svgs.py` 生成（坐标自动计算、可复现），脚本仅写 `diagrams/*.svg`，未触碰引擎源码、`QUESTION.md`、`svg_check.py`。
- 架构事实逐条对照 `include/` 与 `src/` 的源码行号核对（已在上文标注）。

### `run_public_checks.sh` 结果

**① 引擎 demo**：编译运行成功（`g++ -std=c++17 ... -pthread`），demo 自带停止逻辑约 1 秒内自然退出，打印运行时统计，可观测到 worker 线程消费、重试（前 N 次失败后成功）、成功/失败计数与 `[summary] submitted=... succeeded=... failed=... retried=...`。demo 的行为与本文档对「4 worker 线程、重试反馈环、终态通知」的描述一致。

**② SVG 校验**：`svg_check.py` 对 5 张图全部 `[OK]`：
- `01` layered：节点 13（≥6）、边 19（≥4）
- `02` component：节点 12、边 20
- `03` dataflow：节点 9、边 10
- `04` class：节点 20、边 24
- `05` deployment：节点 21、边 19

`公开 SVG 检查全部通过。`

## 剩余风险

- `Scheduler::reschedule`（`scheduler.cpp:112`）功能与重试环内的 `push` 重叠但无人调用，疑似又一处历史遗留；本作答将其归入「死方法」并在陷阱处理中点名，未画入数据流（因运行时不经此路径）。
- `EngineConfig.store_kind` 字段注释为「仅文档性」（`config.hpp:13`），实际装配由组合根决定——已在文档说明，未据此把 `PersistentTaskStore` 画成在用。
- 视觉布局为脚本自动计算，节点密集处（如 04 类关系图）连线较密但语义标注完整、可被机器解析核对。
