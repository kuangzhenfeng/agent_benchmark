# Agent Benchmark Evaluation

> 本轮 run-id：`20260620-0106-cpp-benchmark`；预设 `cpp17-advanced-v1`；5 参评对象 × 3 题；双独立 scorer 盲评。

## 总结

本轮在 3 道 C++17 工程题（组合 Bugfix / 并发实现 / 安全发布重构）上，**glm-5.2 配合两个 Agent（claude code、opencode）双双进入第一梯队**，与 **codex + gpt-5.5** 同处 93–96 区间，三者平均分差 < 2.5 分，未拉开显著差距；而 **qwen3.7max 配合两个 Agent 明显落后**（平均分差 ~8–10 分），差距主要来自「自带验证证据缺失」与「Q3 观察者锁内复制 `std::function`」两点。置信度中等偏高：两位评分者独立得出一致的梯队划分和关键差异点，分差均在复核阈值内。

> 综合平均分 = 两位 scorer 匿名分的算术平均；scorer 全程未见参评身份（详见下文「盲评流程」）。

## 盲评流程

- **匿名 ID**：P01、P02、P03、P04、P05（按 slug 字母序分配，与评分结果无关）。
- **评分员**：两个独立 scorer subagent（scorer-A、scorer-B），各自只读取 `scoring/blind-package/P0x` 与 `scoring/scorer-reference/cpp17-advanced-v1.md`。
- **scorer 是否看到 Agent/模型身份**：**否**。盲评包内未复制含身份的 `agents/<slug>/README.md`，路径不含 slug；对全部 `.cpp/.hpp/.md/.sh` 扫描无身份关键词残留（见 [`scoring/redaction-log.md`](scoring/redaction-log.md)）。
- **未完全匿名的信息**：代码风格本身可能隐含身份（命名/注释/idiom），无法在不破坏技术完整性的前提下清除，列为残余风险；本轮评分均以源码事实和实跑证据为据。
- **复核情况**：最大单题分差 6 分（< 12）、最大总分差 3.6 分（< 8），**均未触发匿名复核阈值**，直接采用双 scorer 平均。

## 总分

> 各题为 scorer-A / scorer-B 的平均分（满分 100）。

| 排名 | 参评对象 | Q1 subscription-hub | Q2 coalescing-cache | Q3 routing-config | 平均分 | 结论 |
|------|----------|:---:|:---:|:---:|:---:|------|
| 1 | claude code + glm-5.2 | 94.5 | 97.5 | 94.5 | **95.5** | 第一梯队，实现扎实且自带验证证据最全 |
| 2 | codex + gpt-5.5 | 90.5 | 95.0 | 95.5 | **93.7** | 第一梯队，Q3 观察者处理全场唯一严格达标 |
| 3 | opencode + glm-5.2 | 94.0 | 93.0 | 93.0 | **93.3** | 第一梯队，验证证据最全（自测项数最多） |
| 4 | claude code + qwen3.7max | 83.5 | 89.0 | 87.5 | **86.7** | 落后，主要因三题均无自带边界测试 |
| 5 | opencode + qwen3.7max | 82.5 | 87.0 | 87.0 | **85.5** | 落后，无自测 + Q3 observer 锁内复制致重入死锁 |

按结论阈值（平均分差）：第一梯队（1–3 名）内部差 < 2.5 分 → **未拉开显著差距**；与第 4–5 名（qwen3.7max）的差距约 **8–10 分** → **存在优势，建议补测确认稳定性**。

## 分题评分（综合两位 scorer）

### Q1 subscription-hub（组合 Bugfix）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|:---:|:---:|:---:|:---:|:---:|:---:|----------|
| claude code + glm-5.2 | 42.5 | 18.5 | 15 | 9 | 10 | 94.5 | shared_ptr+atomic 订阅保活；boundary_checks 7 项实跑通过 |
| codex + gpt-5.5 | 40.0 | 17.0 | 15 | 8 | 8 | 90.5 | shared_ptr 订阅；edge_checks 覆盖；存活检查持锁（非纯原子） |
| opencode + glm-5.2 | 43.0 | 18.0 | 15 | 9 | 10 | 94.0 | shared_ptr+atomic；edge_checks 13 项（含并发）最全 |
| claude code + qwen3.7max | 38.5 | 16.5 | 15 | 7.5 | 5.5 | 83.5 | 值拷贝订阅列表，drain 全量复制 callback；无自测 |
| opencode + qwen3.7max | 38.5 | 16.0 | 15 | 7.5 | 5.5 | 82.5 | drain 全量拷贝 callback、tick 更新 O(N²)；无自测文件 |

> 共性：5 份均在 `unsubscribe`/`expire_idle` 锁内 `erase`，最后一个 `std::function` 引用的析构发生在 mutex 内（参考点名的同类伪修复）。普通 lambda 不触发，析构重入 hub 会死锁——**全场共同短板**，未据此大幅拉分。

### Q2 coalescing-cache（并发实现）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|:---:|:---:|:---:|:---:|:---:|:---:|----------|
| claude code + glm-5.2 | 44.5 | 19.0 | 15 | 10 | 10 | 97.5 | per-key InFlight 独立锁，loader/clock 全锁外；自测 8 项 |
| codex + gpt-5.5 | 44.0 | 18.0 | 15 | 9 | 8 | 95.0 | thread_local 递归检测优雅；但 `size()` 锁内调 `now()` |
| opencode + glm-5.2 | 42.0 | 17.5 | 14 | 9 | 10 | 93.0 | promise/future 合并正确；**`now()` 在 state 锁内调用** |
| claude code + qwen3.7max | 42.0 | 18.0 | 15 | 8 | 5.5 | 89.0 | 状态机正确；无自测；owner 先 notify 后写 ready 的窗口 |
| opencode + qwen3.7max | 42.0 | 16.5 | 14 | 7.5 | 5.5 | 87.0 | 正确；`now()` 锁内调用、手动 list 迭代；无自测 |

> Q2 是差距最小的题（5 份均 87–98）：并发合并状态机本身实现得较扎实。主要拉分点：是否把用户 clock（`now()`）放在锁外（glm-5.2 两个 + codex 做到，opencode+glm 与 opencode+qwen 未做），以及是否自带验证。

### Q3 routing-config（安全发布重构）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|:---:|:---:|:---:|:---:|:---:|:---:|----------|
| codex + gpt-5.5 | 45.0 | 19.0 | 15 | 9 | 8 | 95.5 | **唯一**用 `shared_ptr<const ObserverEntry>`，reload 锁内只拷贝指针，严格满足"锁内不复制/销毁 std::function" |
| claude code + glm-5.2 | 42.5 | 18.5 | 15 | 9 | 9.5 | 94.5 | shared_ptr 发布 + thread_local pin 正确；reload 锁内复制 observers（未验证风险） |
| opencode + glm-5.2 | 44.0 | 18.0 | 15 | 9 | 9.5 | 93.0 | thread_local map pin；自测 16 项最全；observer 锁内复制（未验证风险） |
| claude code + qwen3.7max | 42.0 | 18.0 | 15 | 8 | 5.0 | 87.5 | 发布/pin 正确；observer 锁内复制；无自测 |
| opencode + qwen3.7max | 42.0 | 17.0 | 15 | 8 | 5.0 | 87.0 | shared_mutex 方案可接受；**observer 复制重入触发 EDEADLK 直接 abort**；无自测 |

> Q3 拉出最明显梯度。codex + gpt-5.5 是全场唯一在 Q3 严格满足"observer 复制/析构重入配置 API 不死锁"的（两位 scorer 一致用 copy-reenter probe 验证）。其余 4 份均在 reload 锁内复制 `observers_` 的 `std::function`，其中 opencode + qwen3.7max 因读写锁分离 + 复制重入直接触发系统 `EDEADLK` 崩溃。

## 关键差异

基于源码事实和两位 scorer 的实跑 probe，主要差异为：

1. **自带验证证据（区分度最大）**：glm-5.2（两个 Agent）与 codex+gpt-5.5 在三题均提供 boundary/edge_checks 并实跑通过（验证维度 8–10 分）；qwen3.7max（两个 Agent）三题均无自带边界测试、仅靠公开检查（验证维度 5 分左右）。这是第一梯队与 qwen3.7max 约 8–10 分差距的主因，证据确凿（`tests/` 目录文件清单 + ANSWER.md 自承）。

2. **Q3 观察者锁边界**：codex + gpt-5.5 唯一把 observer 封装为 `shared_ptr<const ObserverEntry>`、reload 锁内只拷贝指针（两位 scorer 的 copy-reenter probe 唯一通过）；其余 4 份锁内复制 `std::function`，opencode + qwen3.7max 更触发 `EDEADLK` 崩溃。这是 Q3 内的硬性技术差异。

3. **Q2 用户 clock 调用位置**：claude code+glm-5.2 与 codex 把 `now()` 放在锁外；opencode+glm-5.2 与 opencode+qwen3.7max 在 state 锁内调用 `now()`（与"loader 锁外执行、不在 mutex 内执行用户代码"的精神冲突，clock 若重入会死锁）。

4. **Q1 订阅保活设计**：glm-5.2（两个 Agent）用 shared_ptr+atomic，开销小；qwen3.7max（两个 Agent）用值拷贝订阅列表，drain 时 N 次 std::function 复制、tick 更新 O(N²)，正确但效率/封装偏弱。

5. **全场共性短板**：Q1 的 `unsubscribe`/`expire_idle` 锁内 erase 导致最后一个 `std::function` 在 mutex 内析构——5 份全部存在，dtor-reenter probe 下均死锁。这是题目故意埋的拉分点，但因全员踩中，未据此拉开分差。

## 风险与限制

- **样本量**：5 参评对象 × 3 题 = 15 份提交，单轮单类题型，结论**不应外推**到所有任务、所有模型版本或不同领域。
- **作答时间起点不均**：qwen3.7max（两个对象）先开始作答、用时可能更长；glm-5.2 / codex-gpt-5.5 后加入。这是用户临时补充造成的分发时序，不影响匿名性，但影响时间公平性——无法排除"先答者反而在验证上投入更少"等解释，需谨慎归因到模型能力本身。
- **未运行/无法确认的项**：部分并发缺陷（callback/observer 析构重入死锁、clock 重入死锁）属极端场景，两位 scorer 用自编 probe 验证为死锁/超时，但参评者自测未必覆盖；这类风险已在「正确性/验证」维度相应扣分。
- **残余匿名性风险**：代码风格可能隐含身份，本轮不伪装为完全双盲。
- **环境固定性**：仅本机 g++ 9.4.0 / clang++，Linux x86_64（WSL2），未跨平台/跨编译器。
- **阈值核对**：双 scorer 最大单题差 6（< 12）、最大总分差 3.6（< 8），未触发匿名复核，结论稳健性中等偏高。

## 下一轮建议

- **聚焦"验证"维度差异**：本轮 qwen3.7max 落后的主因之一是"未提供自带边界测试"。下一轮可在题面更强调"必须提交并发测试并通过"，或直接将该维度权重上调，以区分是"实现能力"还是"验证自觉性"的差距。
- **补测稳定性**：第一梯队（glm-5.2 ×2 + codex+gpt-5.5）平均分差 < 2.5 分，未拉开显著差距。若要在这三者间分出高下，建议增加题量（如 +1–2 道更难的并发题）或聚焦单一任务类型多轮复测。
- **控制时间公平**：下一轮应在分发前一次性确定全部参评对象，避免分批补充导致作答时长不均。
- **可选拆解**：本轮可观察到 Agent 接入（claude code vs opencode）与模型（glm-5.2 vs qwen3.7max）存在交叉，但 5 个样本不足以做 Agent/模型正交归因；如需拆解，建议设计 2×2 + 重复的更大样本 benchmark。
