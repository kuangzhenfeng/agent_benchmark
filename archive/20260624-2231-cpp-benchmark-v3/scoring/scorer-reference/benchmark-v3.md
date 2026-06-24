# benchmark-v3 私有评分参考

仅在全部参评对象停止作答后复制给匿名评分员。不得进入公共题源、参评目录或作答前交付物。

本文件是架构事实的语义校准与验收矩阵，不是唯一画法。只要 5 张 SVG 表达的架构事实与源码一致、标注约定合规、行为等价，必须接受不同的布局/配色/形状选择。私有 ground truth（`q01-batch-engine-architecture/_private/q01_truth.json`）与本文件配套，二者须一致。

## 评测对象

一套手写 C++17 批处理引擎 `TaskForge`（`include/` + `src/`，可编译运行）。参评对象只读源码，产出 5 张 SVG。正确性靠参评对象按约定嵌入的 `data-*` 标注与 ground truth 做集合匹配来客观核对。

## 5 视角的验收矩阵

每个视角都有一组"必须表达/必须正确"的架构事实；以下给出关键审查点与常见错误。完整节点/边清单见 ground truth JSON。

### ① 分层架构（`data-style="layered"`）

- 按**真实编译期依赖方向**分层：application（main/组合根）→ orchestration（Scheduler/WorkerPool/Worker）→ domain（TaskQueue/Task/TaskResult/重试策略族）→ infrastructure（存储/通知/时钟/日志/配置契约与实现、MetricsCollector）。
- **关键审查点**：`MetricsCollector` 头注释自称"核心调度组件"，但按依赖方向是叶子（只含 `<atomic>`、不依赖任何内部模块），属 infrastructure 层。把它画进 orchestration/核心层视为分层错误。
- 常见错误：按注释自称分层而非按依赖方向分层；把死代码 `TaskQueueLegacy` 画入核心层。

### ② 组件依赖（`data-style="component"`）

- 编译期依赖：`main→Scheduler`、`Scheduler→{WorkerPool,TaskQueue,ITaskStore,IRetryPolicy,INotifier,MetricsCollector,IClock,EngineConfig}`、`WorkerPool→Worker`、`Worker→{TaskQueue,Task,TaskResult}`。
- **关键审查点（伪循环依赖）**：`Worker→Scheduler` 仅是运行时回调（经 `set_completion_handler` 注入的完成处理器调 `handle_completion`）；`worker.hpp` 不 include `scheduler.hpp`，无编译期环。把它画成编译期双向循环依赖视为错误，应标 `callback`/运行时。
- 常见错误：把运行时回调等同于编译期依赖；漏画 Scheduler 对各接口契约的依赖。

### ③ 数据流（`data-style="dataflow"`）

- 一次任务流转：Client `submit/submitBatch(Task)` → Scheduler 入队（按优先级）+ 存储初值 → Worker `blocking_pop` → 执行 `work()` → 产出 `TaskResult` → 经完成回调回 Scheduler → 失败时询问 `IRetryPolicy` 是否还有退避 → 重试入队（attempt++）或终态 → 更新存储状态、记指标、通知。
- **关键审查点**：必须包含失败重试反馈环 `Worker→Scheduler(on_done)→IRetryPolicy?→Scheduler→TaskQueue(re-enqueue)`；`submit` 与 `submitBatch` 是两条独立入口，不得合并。
- 常见错误：漏掉重试回路；把完成回调画成普通同步调用而忽略其运行时性质。

### ④ 类关系（`data-style="class"`）

- 继承/实现（9 条）：`InMemoryTaskStore`/`PersistentTaskStore`→`ITaskStore`；`FixedBackoffPolicy`/`ExponentialBackoffPolicy`→`IRetryPolicy`；`ConsoleNotifier`/`CallbackNotifier`/`CompositeNotifier`→`INotifier`；`SystemClock`→`IClock`；`ConsoleLogger`→`ILogger`。
- 组合/聚合：`main◇—Scheduler`、`Scheduler◇—WorkerPool`、`Scheduler◇—TaskQueue`、`WorkerPool◇—Worker(×N)`、`CompositeNotifier◇—INotifier(*)`。
- **关键审查点（接口 vs 实现）**：Scheduler 持 `shared_ptr<ITaskStore>`/`unique_ptr<IRetryPolicy>`/`shared_ptr<INotifier>`，依赖边应画到**接口**；具体实现（InMemoryTaskStore/ExponentialBackoffPolicy/CompositeNotifier 等）由组合根（main）装配。画 `Scheduler→InMemoryTaskStore`（把接口依赖替换为具体类依赖）视为错误。
- 常见错误：把依赖画到具体实现而非接口；漏画 CompositeNotifier 的组合模式聚合。

### ⑤ 部署/运行时拓扑（`data-style="deployment"`）

- 实例与数量：main-thread(1)、Scheduler(1)、WorkerPool(1)、Worker(N=`EngineConfig::worker_count`，默认 4)、TaskQueue(1,共享,mutex+cv)、ITaskStore→InMemoryTaskStore(1)、IRetryPolicy→ExponentialBackoffPolicy(1)、INotifier→CompositeNotifier(1,内含 Console+Callback)、MetricsCollector(1)、IClock→SystemClock(1)。
- 归属：main-thread⊃Scheduler⊃WorkerPool⊃Worker(×N)；Scheduler⊃TaskQueue。
- 连接：Scheduler→TaskQueue(push)、Worker→TaskQueue(blocking pop on cv)、Worker→Scheduler(completion 回调,运行时)、Scheduler→{ITaskStore,MetricsCollector,INotifier,IRetryPolicy}。
- **关键审查点**：`TaskQueue` 是 Scheduler 与所有 Worker 共享的汇合点；编译进产物但运行时未实例化的有 `PersistentTaskStore`、`FixedBackoffPolicy`、`TaskQueueLegacy`、`Scheduler::run_legacy`——画入运行时拓扑或标"未实例化/遗留"均可，但不得把死代码当作活跃运行时实例。
- 常见错误：worker 数量画错或漏标"共享队列"；把未实例化的具体类画成运行时实例。

## 陷阱审查点（5 类，区分度核心）

| 陷阱 | 正确判断 | 错误（扣分）来源 |
|------|----------|------------------|
| 近似命名 | `TaskQueue` 是活跃优先级队列；`TaskQueueLegacy` 是 FIFO 死代码，全仓库无调用 | 把两者混用 / 把 Legacy 当活跃 |
| 死代码 | `TaskQueueLegacy`、`Scheduler::run_legacy()` 无人调用，标遗留或不入核心架构 | 当作核心组件画入 |
| 伪循环依赖 | `Worker→Scheduler` 仅运行时回调（worker.hpp 不 include scheduler.hpp） | 画成编译期循环 |
| 分层误导 | `MetricsCollector` 属 infrastructure（叶子），不因注释自称"核心"归入编排层 | 按注释分层 |
| 接口 vs 实现 | Scheduler 依赖 ITaskStore/IRetryPolicy/INotifier 接口，具体类由 main 装配 | 依赖画到具体类 |

## machine-grade.json 的使用

- 主 agent 运行 `grade_svg.py` 产出 `machine-grade.json`：每个匿名提交有 5 视角的节点/边 precision/recall/f1、视角得分（0.6×节点f1+0.4×边f1）、加权 `coverage_raw_out_of_45`、陷阱命中标志（`layering_present`/`no_compile_cycle`/`interface_deps`）与 `trap_bonus`、最终 `coverage_score_out_of_45`。
- **正确性维度（45 分）**：以 `coverage_score_out_of_45` 为客观主轴，scorer 可在其上下小幅调整（如发现标注归一化未覆盖的命名、或机器漏判的明显错误），但不得无故偏离。
- 机器核对的是"标注是否对得上源码事实"，**不评价图的视觉质量**——视觉质量由 scorer 在 SVG 产物质量维度（20 分）主观给分。

## 评分使用规则

- 先以公开 `QUESTION.md` 与源码为准；本文件与 ground truth JSON 只帮助识别看似合理但与源码不符的表达。
- 视觉风格（配色/布局/形状/字体/装饰）的差异不影响正确性，只要 `data-*` 标注的架构事实正确；不得以"不够好看"扣正确性分。
- 遇到机器核对未覆盖、scorer 又无法从源码确定的判断，明确标注为未验证风险并相应扣减"验证证据"，不得臆测通过。
- 本参考不得出现在公开题源、参评目录、作答前交付或面向参评者的最终报告中。
