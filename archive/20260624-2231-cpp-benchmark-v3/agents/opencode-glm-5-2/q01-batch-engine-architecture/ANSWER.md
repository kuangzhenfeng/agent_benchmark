# 作答说明

## 架构推理概述

TaskForge 是一个**进程内**的任务调度 / 批处理引擎，典型依赖注入（DI）+ 组合根（composition root）结构：

- **组合根在 `main.cpp`**：`main` 是全仓库唯一知道"具体实现类型"的地方。它在 `main.cpp:21-39` 装配 `InMemoryTaskStore`、`ExponentialBackoffPolicy`、`MetricsCollector`、`CompositeNotifier`（内含 `ConsoleNotifier`+`CallbackNotifier`）、`SystemClock`、`EngineConfig`，再以抽象接口指针/引用传给 `Scheduler`。
- **`Scheduler` 是编排核心**（`scheduler.hpp:19`、`scheduler.cpp`）：自身只持有 `ITaskStore*`、`IRetryPolicy*`、`INotifier*`、`MetricsCollector*`、`IClock&` 等抽象，并以**值语义**拥有 `TaskQueue queue_` 与 `WorkerPool pool_`。`submit()` 分配 id → 存库 → 入队 → 计数；`run()` 阻塞主循环；`handle_completion()` 是 Worker 结果的结算点（成功 / 失败 / 重试三分支）。
- **`WorkerPool` / `Worker` 是消费侧**：每个 `Worker` = 一个线程，`loop()` 短超时 `blocking_pop` 任务、执行 `task.work()`、产出 `TaskResult`，通过**运行时注入的完成回调**回报，自身不依赖 `Scheduler` 类型（`worker.hpp` 不 include `scheduler.hpp`）。
- **状态机**：`Pending → Queued → Running → (Retrying ↺) → Succeeded/Failed`（`task.hpp:9-17`），所有状态落库由 `ITaskStore` 承担。
- **线程模型**：主线程跑 `scheduler.run()`；一个"停止线程"在 800ms 后 `stop()`；`WorkerPool` 默认 4 个 Worker 线程。

## 各视角的关键节点 / 边与源码依据

### 01 分层架构图（`data-style="layered"`，边仅用 `depends`）

按**真实编译期 #include 方向**分 4 层（上层依赖下层，下层不感知上层）：

| 层 | 节点 | 依据 |
|----|------|------|
| L3 应用入口 | `main` | `main.cpp` 是唯一组合根 |
| L2 编排 | `Scheduler` | `scheduler.hpp` include 几乎所有领域/基础设施头 |
| L1 领域服务 | `WorkerPool`/`Worker`/`TaskQueue`/`Task`/`ITaskStore`/`INotifier`/`IRetryPolicy` | `worker.hpp`、`task_queue.hpp`、`task_store.hpp`、`notifier.hpp`、`retry_policy.hpp` 互为领域模块 |
| L0 基础设施 | `MetricsCollector`、`IClock/SystemClock`、`EngineConfig/ConfigLoader`、`ILogger/ConsoleLogger`、`TaskStatus` | 均无内部依赖的叶子工具 |

关键 `depends` 边均来自 `#include`：`scheduler.hpp:3-10`、`worker.hpp:3-4`、`task_queue.hpp:3`、`task_store.hpp:3`、`notifier.hpp:3`、`main.cpp:1-6`。

**陷阱处理**：`MetricsCollector` 注释自称"核心调度组件：引擎核心调度与统计"（`metrics.hpp:7`），但它无任何内部依赖、只是原子计数器，按依赖方向归入 **L0 基础设施**，而非 L2 编排。

### 02 组件依赖图（`data-style="component"`，边用 `depends`/`callback`）

节点 = 模块（头文件）。`depends` 边严格取自 `#include` 关系：

- `scheduler.hpp` → `task`/`task_store`/`task_queue`/`retry_policy`/`metrics`/`clock`/`worker`/`config`；`scheduler.cpp` 额外 → `notifier`（`scheduler.cpp:2`）。
- `worker.hpp:3-4` → `task`、`task_queue`（**不**含 `scheduler`）。
- `main.cpp:1-6` → `scheduler`/`notifier`/`metrics`/`clock`/`config`/`logger`。

**陷阱处理（运行时回调 vs 编译期依赖）**：图中唯一一条 `callback` 边是 `worker → scheduler`（橙色虚线），对应 `scheduler.cpp:48-49` 的 `pool_.set_completion_handler([this](const TaskResult& r){ handle_completion(r); })`。这是**运行时注入的回调**，不构成编译期循环依赖——证据是 `worker.hpp` 不 include `scheduler.hpp`，Worker 只持有 `std::function<void(const TaskResult&)>`。

### 03 数据流图（`data-style="dataflow"`，边用 `flow`）

一次任务的完整流转（源码锚点）：

1. `Client → Scheduler.submit`（`scheduler.cpp:22`）：分配 `TaskId`、`created_tick`、`status=Queued`。
2. `submit → ITaskStore.save`（`scheduler.cpp:27`）与 `submit → TaskQueue.push`（`scheduler.cpp:28`）、`submit → MetricsCollector.record_submit`（`scheduler.cpp:29`）。
3. `TaskQueue → Worker.loop`（`worker.cpp:38` `blocking_pop`）。
4. `Worker.loop → task.work`（`worker.cpp:51`）→ 回 `TaskResult`（`worker.cpp:42-66`）。
5. `Worker.loop → handle_completion`（`worker.cpp:69-71` 经回调；结算在 `scheduler.cpp:82`）。
6. 成功：`update_status(Succeeded)`→`record_success`→`notify("task.succeeded")`→`remove`（`scheduler.cpp:85-89`）。
7. 失败：先问 `IRetryPolicy.next_delay_ms`（`scheduler.cpp:93-94`）。
   - **重试反馈环**：仍有次数 → `record_retry` → `load` → 同步 `attempt`（`scheduler.cpp:105` 注释强调否则永不递增）→ `status=Retrying` → **重新 `push` 回 `TaskQueue`**（`scheduler.cpp:102-109`，图中红色虚线反馈环）。
   - 放弃：`update_status(Failed)`→`record_failure`→`notify("task.failed")`（`scheduler.cpp:95-98`）。
8. `INotifier → Terminal`（Succeeded/Failed）。

### 04 类与模块关系图（`data-style="class"`，边用 `implements`/`composes`/`aggregates`/`uses`/`depends`）

- **implements**：`InMemoryTaskStore`/`PersistentTaskStore` → `ITaskStore`（`task_store.hpp:22,36`）；`ExponentialBackoffPolicy`/`FixedBackoffPolicy` → `IRetryPolicy`（`retry_policy.hpp:17,29`）；`SystemClock` → `IClock`（`clock.hpp:14`）；`ConsoleNotifier`/`CallbackNotifier`/`CompositeNotifier` → `INotifier`（`notifier.hpp:18,24,35`）；`ConsoleLogger` → `ILogger`（`logger.hpp:14`）。
- **composes（值语义拥有）**：`Scheduler` composes `TaskQueue`、`WorkerPool`（`scheduler.hpp:55-56` 成员）；`WorkerPool` composes `Worker`（`worker.hpp:48` `vector<unique_ptr<Worker>>`）。
- **aggregates（引用/共享）**：`Scheduler` aggregates `ITaskStore`/`IRetryPolicy`/`INotifier`/`MetricsCollector`（`scheduler.hpp:48-51`，均为 `shared_ptr`/`unique_ptr` 接口）；`CompositeNotifier` aggregates `INotifier`（组合模式，`notifier.hpp:41` `vector<shared_ptr<INotifier>>`）。
- **uses（不持有）**：`Scheduler` uses `IClock`（`scheduler.hpp:52` 裸引用）；`Worker` uses `TaskQueue`/`Task`。

**陷阱处理（接口依赖 vs 具体类依赖）**：`Scheduler` 的依赖边一律画到**抽象接口**（`ITaskStore`/`IRetryPolicy`/`INotifier`/`IClock`），不画到具体实现。具体实现（`InMemoryTaskStore`、`ExponentialBackoffPolicy`、`CompositeNotifier`、`SystemClock`、`MetricsCollector`）由组合根 `main` 用 `depends` 边装配（`main.cpp:21-39`）。运行时未实例化的实现（`PersistentTaskStore`、`FixedBackoffPolicy`、`ConsoleLogger`）以灰色虚线框标出。

### 05 部署 / 运行时拓扑图（`data-style="deployment"`，边用 `contains`/`connects`/`runs-on`/`callback`）

单进程拓扑，所有实例标注运行时数量（`×N`）：

- **进程** `TaskForge ×1`，内含：
  - **主线程 ×1**（跑 `scheduler.run()`）、**停止线程 ×1**（800ms 后 `stop()`，`main.cpp:73-76`）。
  - `Scheduler ×1`、`WorkerPool ×1`、**Worker 线程 ×4**（`EngineConfig.worker_count{4}`，`config.hpp:9`；demo 用 `default` 档位，`config.cpp:13-15`）。
  - `TaskQueue ×1`：**共享汇合点**，主线程 push、4 个 Worker blocking_pop，`mutex+condition_variable` 同步（`task_queue.hpp:35-37`）。
  - 装配的单例基础设施：`InMemoryTaskStore ×1`、`ExponentialBackoffPolicy ×1`(50/2000/cap3)、`CompositeNotifier ×1`（含 `ConsoleNotifier ×1`+`CallbackNotifier ×1`）、`MetricsCollector ×1`、`SystemClock ×1`。
- **callback** 边：Workers → `Scheduler.handle_completion`（运行时回调）。
- **编译进产物但运行时未实例化**（图右侧灰色备查框，**不**画为实例节点）：`PersistentTaskStore`、`FixedBackoffPolicy`、`ILogger/ConsoleLogger`、`TaskQueueLegacy`、`Scheduler::reschedule()`、`Scheduler::run_legacy()`。

## 陷阱处理（各举一例 + 源码证据）

1. **命名相近的活跃类型 vs 死代码旧类型**：`TaskQueue`（`task_queue.hpp:13`，`priority_queue` 实现，被 `Scheduler`/`Worker` 实际使用）是**在用**的；`TaskQueueLegacy`（`task_queue.hpp:42`，注释自称"通用任务队列，引擎各处使用"）经全仓库检索**无任何调用方**——是死代码。所有数据流/部署图只画 `TaskQueue`，`Legacy` 仅在部署图"未实例化"框中备查。
2. **死代码 / 历史遗留**：`Scheduler::reschedule()`（`scheduler.cpp:112`）定义完整但全仓库无人调用（重试逻辑内联在 `handle_completion:101-109`）；`Scheduler::run_legacy()`（`scheduler.cpp:66`）只是委托给 `run()` 的兼容壳，`main` 用的是 `run()`。二者均不计入运行时拓扑。
3. **运行时回调 vs 编译期循环依赖**：见 02 节——Worker→Scheduler 仅一条 `callback` 虚线边，依据 `scheduler.cpp:48-49` 与 `worker.hpp` 不 include `scheduler.hpp`。绝不画成编译期循环。
4. **注释自称与真实分层不符**：`MetricsCollector` 注释"核心调度组件：引擎核心调度与统计"（`metrics.hpp:7`），但它是无内部依赖的原子计数叶子——归入 L0 基础设施层，而非 L2 编排层。
5. **接口依赖 vs 具体类依赖**：见 04 节——`Scheduler` 依赖边画到 `ITaskStore`/`IRetryPolicy`/`INotifier`/`IClock` 抽象；具体实现由 `main` 组合根装配。此外 `main.cpp:6` 虽然 `#include "logger.hpp"`，但**从未实例化** `ConsoleLogger`——属编译期依赖但运行时未用，部署图未画其实例。

## 验证

- **SVG 合法性**：运行 `python3 svg_check.py diagrams`，5 张图全部 `[OK]`（well-formed XML、根为 `<svg>`、有 viewBox、`data-style` 合法、`nodes`/`edges` 容器齐、节点≥6、边≥4）。各图节点/边数：01=14/17、02=12/21、03=11/14、04=21/28、05=16/20。
- **完整公开检查**：运行 `./run_public_checks.sh`（见下方"实际运行结果"），demo 编译运行成功，SVG 检查全过。
- **绘图方式**：SVG 全部**手写为静态 XML**（无外部依赖、无脚本），直接落在 `diagrams/*.svg`，可复现：打开即可渲染，`xml.etree.ElementTree` 可解析。
- **架构事实核对**：逐条与 `include/`、`src/` 对照（依赖方向取自 `#include`、运行时数量取自 `config.hpp:9` 与 `main.cpp`、回调关系取自 `scheduler.cpp:48-49` 与 `worker.cpp:69-71`）。

> 实际运行结果见终端输出的"=== 1. 编译并运行 TaskForge demo ==="与"=== 2. 校验 5 张 SVG ==="两段，末尾打印"公开 SVG 检查全部通过。"

## 剩余风险

- 视觉布局（坐标、配色）是人工排版，个别跨层连线在密集区可能略有重叠，但不影响 `data-*` 标注的机器核对正确性。
- `Task` 在纯依赖图里是"叶子"，但语义上属领域实体——分层图按题面"按依赖方向分层"的精神放在领域层，同时承认其依赖-叶子性质；此为题面允许的判断空间。
- `EngineConfig.store_kind` 字段在 `config.hpp:13` 自称"仅文档性，实际装配由组合根决定"——已据此只承认 `InMemoryTaskStore` 为运行时实际存储。
