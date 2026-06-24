# Blind Scoring Package — 20260620-1012-cpp-benchmark

本目录是给 **盲评评分员（scorer subagent）** 的匿名评分包。

## 你的角色与限制

你是 C++ benchmark 的盲评评分员。

- 你只能读取本目录 `blind-package/P01`…`P06`，以及 `scoring/scorer-reference/benchmark-v1.md`（私有参考解答）。
- 你**不知道、也不需要知道** `P01`…`P06` 分别对应哪个 Agent 或模型。不要尝试推断身份。
- **禁止**读取以下目录：`mapping.private.md`、`participants.md`、`agents/`、`submissions/`、`questions/`（题源原件）、`.claude/`。
- 评分只使用匿名 ID `P01`…`P06`。
- **本轮追加盲评**：`P06` 为后续追加的参评对象提交，`P01`–`P05` 已归档裁决、分数不再变。若本轮只评 `P06`，只输出 `P06` 即可，不要重新评 `P01`–`P05`。

## 目录结构

每个 `Pxx/` 下三个题目子目录，结构相同（`P06` 仅有 `q01`–`q03`，无 README）：

```text
Pxx/
├── q01/   # SubscriptionHub（组合 Bugfix，目标 50 min）
│   ├── QUESTION.md        ← 题面与验收标准（公共，所有 Pxx 同源）
│   ├── ANSWER.md          ← 该参评对象的作答说明
│   ├── include/           ← 该参评对象修改后的头文件
│   ├── src/               ← 该参评对象修改后的实现
│   ├── tests/             ← 公开测试 + 该参评对象可能新增的测试
│   └── run_public_checks.sh
├── q02/   # CoalescingCache（Implementation，目标 70 min）
└── q03/   # RoutingConfig（Refactor/Design，目标 60 min）
```

> 注：`QUESTION.md` 描述的是原始 starter 代码的语义契约。参评对象已在此基础上修改 `include/`、`src/`、`tests/`。请以 `QUESTION.md` 的验收标准 + 参评代码 + 验证记录为准评分。

## 评分 Rubric

每题满分 **100 分**，按 5 个维度：

| 维度 | 分值 | 评分标准 |
|------|------|----------|
| 正确性 | 45 | 是否满足 `QUESTION.md` 要求；核心行为与边界条件是否正确；并发/重入/生命周期/异常等组合语义是否处理 |
| C++ 质量 | 20 | 是否合理使用 C++17（RAII、`const`、`shared_ptr`/`atomic`、移动语义、类型边界）；错误处理 |
| 约束遵循 | 15 | 是否遵守题面限制、提交格式；未读取其他参评对象结果；未破坏题目结构 |
| 可维护性 | 10 | 简洁清晰、职责明确；无过度封装或无关重写 |
| 验证证据 | 10 | 是否提供可信测试、命令输出、推理或人工验证说明；未编造不存在的文件/命令 |

### 硬性扣分（触发分数上限）

| 情况 | 处理 |
|------|------|
| 未提交核心代码 | 该题总分 ≤ 30 |
| 编造测试、命令或不存在的文件 | 该题总分 ≤ 40 |
| 读取/引用其他参评对象结果 | 该题总分 ≤ 50 |
| 违反 `QUESTION.md` 中标记为「硬性封顶条件」的禁止事项（仅 Q1 callable 生命周期锁边界） | 该题总分 ≤ 60 |
| 大量无关重写导致无法审查 | 可维护性 = 0，正确性按可验证部分给分 |
| 破坏题目结构导致无法判断 | 该题总分 ≤ 50 |

### 各题关键验收点（摘要，详见各 `QUESTION.md` 与 `scorer-reference`）

- **Q1 SubscriptionHub**：topic/payload 由 hub 自有（`std::string`，非悬挂 view）；`drain` 锁内截取队列并冻结订阅顺序、锁外回调；`subscribe` 用 `shared_ptr<Callback>`，`std::function` 的复制/移动/销毁绝不在 mutex 内（**硬性封顶条件**）；callback 取消/重入/发新事件不死锁且不污染当前批次；异常不重放、其余回调继续；TTL 用 `uint32_t` 无符号模减法（含 `UINT32_MAX-2 → 3` 回绕）。
- **Q2 CoalescingCache**：N 个并发同 key miss → loader 恰好 1 次；waiter 等待而非重复加载；Clock 与 loader 在 cache mutex 外调用（不死锁）；同线程同 key 重入 → `recursive_load`；loader 异常 → 全部 waiter 失败且不缓存；TTL=0 不写 ready；in-flight 中 `invalidate` → 当前 flight 返回但不缓存；晚到调用安全；容量淘汰只踢 ready entry、LRU 正确。
- **Q3 RoutingConfig**：无效 candidate 不发布/不递增版本/不通知；reload 发布 `shared_ptr<const Config>` 快照（原子/短锁）；`current()` 用 per-instance `thread_local` pin 保证旧引用安全（跨实例不互相覆盖）；`subscribe` 用 `shared_ptr<Observer>`、锁外通知、`std::function` 不在 mutex 内复制/销毁；observer 重入/异常不破坏通知集合；最长前缀匹配；并发 reload/read 只见完整单一版本。

## 评分要求

1. **逐题、逐维度**给分，每个分数都要**引用题面要求 + 源码事实（文件:行/函数）+ 验证证据**。
2. **行为等价优先**：接受任何满足公开题面语义、资源安全、并发约束的实现，不以参考解答的类名/锁类型/容器选择扣分。
3. **可判定优先**：能从源码或验证记录确定的行为才给满分；无法确定的并发行为标注「未验证风险」并相应扣减「验证证据」，**不得臆测通过**。
4. **识别伪修复**：对照 `scorer-reference` 的验收矩阵，警惕只让公开测试通过、但组合语义仍错的实现（如锁内复制 `std::function`、返回内部可变引用、用单 TLS pin 被跨实例覆盖等）。
5. 你可以**编译并运行** `run_public_checks.sh` 或自写测试来验证（用 `g++ -std=c++17`）；但要把实际命令与输出作为证据记录，**不要编造**。
6. 注意区分参评对象**自行新增的测试**（如 `extra_*`/`edge_*`）与公共测试：公共测试通过是基线，自写测试反映验证意识。

## 输出格式

输出一份 **Markdown 报告**，结构如下，写入你的最终回复（不要写进 blind-package）：

```markdown
# 盲评评分报告

## 总分表

| Pxx | Q1/100 | Q2/100 | Q3/100 | 总分/300 |
|-----|--------|--------|--------|----------|
| P01 | ... | ... | ... | ... |
...

## Q1 SubscriptionHub

### P01
- 正确性 45 → __/45：[依据，引用文件:行]
- C++质量 20 → __/20：[依据]
- 约束遵循 15 → __/15：[依据]
- 可维护性 10 → __/10：[依据]
- 验证证据 10 → __/10：[依据]
- 小计：__/100
- 硬性扣分触发：[是/否，说明]
- 关键缺陷：[...]
（P02–P05 同结构）

## Q2 CoalescingCache
（同上）

## Q3 RoutingConfig
（同上）

## 关键差异观察（基于匿名 ID，不外推身份）
- ...

## 评分不确定 / 未验证项
- ...
```

只输出匿名报告。完成后停止，不要尝试推断或询问身份。
