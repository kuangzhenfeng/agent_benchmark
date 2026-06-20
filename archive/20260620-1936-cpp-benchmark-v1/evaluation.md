# Agent Benchmark 评测报告 · 20260620-1936-cpp-benchmark-v1

> 预设 `cpp17-advanced-v1`（组合 Bugfix / 实现 / 重构，C++17）。6 个 opencode coding 模型经 `xag` 接入，3 题。

## 匿名映射（仅本报告解盲用）

| 匿名 | 参评对象 | 匿名 | 参评对象 |
|------|----------|------|----------|
| P01 | coding-deepseek | P04 | coding-minimax |
| P02 | coding-glm | P05 | coding-qwen |
| P03 | coding-kimi | P06 | coding-qwen-preview |

## 总结

本轮 6 个 opencode coding 模型在 v1 三道高级工程题上呈现**清晰的梯队分化**：

- **第一梯队：coding-qwen-preview（P06，281）与 coding-glm（P02，279）并列领先**，差距仅 2 分；三项并发公开检查全部稳定通过，源码正确处理 callable 生命周期、drain 批次冻结、flight 合并、per-instance TLS pin、锁外通知等组合维度。
- **coding-deepseek（P01，265）紧随**，公开检查全过但 Q1/Q3 各有一处锁边界缺口（未触发但属真实并发隐患）。
- **第二梯队：coding-minimax（P04，187）与 coding-qwen（P05，182）**，各有确定性死锁。minimax 在 Q1 死锁且改了公共 API；qwen 在 Q1、Q3 死锁。
- **第三梯队：coding-kimi（P03，141）三题全部确定性死锁**，最严重。

> **更正说明**：初稿曾误将 P02/P05 的模型名标反。正确映射为 P02=glm（第二名）、P05=qwen（第五名）。本版已据 `mapping.private.md` 全量更正。

结论置信度**高**：客观公开检查锚点（前三 {P01/P02/P06} 全过 vs 后三 {P03/P04/P05} 死锁）与两名独立盲评评分员分组判定完全一致。

## 盲评流程

- 匿名 ID：P01..P06（按 slug 字母序分配，仅主 agent 用于解盲）
- 评分员：两个独立 scorer subagent（A 严、B 松），均只读 `blind-package/` + `scorer-reference/`，均确认未读取身份信息
- 主 agent 额外对每个匿名对象逐项跑 no-ASAN 公开检查 3 次作为客观锚点（`scoring/reconcile.md`）
- 主 agent 源码级核验了 A 的两个独有发现（P04 改公共 API、P01 drain 锁内拷贝 Callback）均属实
- 两评分员总分差异 >8 分阈值的项，差异系统性来自严格度（硬封顶区间内的 30-40 vs 56 均合法）而非事实分歧；前三/后三分组两评分员一致且与客观锚点吻合
- 取两评分员平均分作为最终题分

## 总分（300 满 = Q1 100 + Q2 100 + Q3 100，两名评分员平均）

| 排名 | 参评对象 | 匿名 | Q1 | Q2 | Q3 | 总分 | 结论 |
|------|----------|------|----|----|----|------|------|
| 1 | opencode + coding-qwen-preview | P06 | 94 | 94 | 93 | **281** | 三题公开检查全过；reload 用局部 new_snapshot 通知（无 published_ 锁外读竞争），最贴近参考 |
| 2 | opencode + coding-glm | P02 | 95 | 92 | 92 | **279** | 三题全过；shared_ptr<Callback> 锁外构造 + drain 批次冻结，附自测 edge_case |
| 3 | opencode + coding-deepseek | P01 | 88 | 93 | 84 | **265** | 三题全过；Q1 drain 锁内拷贝 Callback、Q3 reload 锁外读 shared_ptr 两个验收缺口 |
| 4 | opencode + coding-minimax | P04 | 43 | 74 | 70 | **187** | Q1 callable_lifetime **确定性死锁** + 改公共 API；Q2 矩阵缺口 |
| 5 | opencode + coding-qwen | P05 | 48 | 90 | 44 | **182** | Q1、Q3 **确定性死锁**；Q2 全过；裸指针+只增 history、ANSWER 编造 thread_pins |
| 6 | opencode + coding-kimi | P03 | 48 | 47 | 46 | **141** | **三题全部确定性死锁**（Q1 callable、Q2 clock、Q3 observer），最严重 |

> 各维度满分：正确性 45 / C++ 质量 20 / 约束 15 / 可维护性 10 / 验证 10（每题 100）。

## 分题评分（两名评分员平均题分；维度拆分见 `scorer-report-A.md` / `scorer-report-B.md`）

### Q1 subscription-hub（组合 Bugfix）

| 参评对象 | 匿名 | 公开检查 | 题分 | 主要依据 |
|----------|------|----------|------|----------|
| coding-glm | P02 | callable ✅×3 | **95** | shared_ptr<Callback> 锁外构造；drain 锁内截取 + max_original_id 冻结批次；TTL 模减；附 edge_case 自测 |
| coding-qwen-preview | P06 | callable ✅×3 | **94** | 所有权/批次/异常/TTL 正确 |
| coding-deepseek | P01 | callable ✅×3 | **88** | drain 行45 锁内拷贝 Subscription 向量（含 Callback），违反第7条但未触发 |
| coding-qwen | P05 | callable ⏰×3 | **48** | 确定性死锁（锁内销毁/拷贝 Callback→析构重入）；触发第7条硬封顶≤60 |
| coding-kimi | P03 | callable ⏰×3 | **48** | 同类死锁 |
| coding-minimax | P04 | callable ⏰×3 | **43** | 死锁 + 改公共 API（Event `string_view`→`string`），违反「不得改变 Event 公共 API」 |

### Q2 coalescing-cache（Implementation）

| 参评对象 | 匿名 | 公开检查 | 题分 | 主要依据 |
|----------|------|----------|------|----------|
| coding-qwen-preview | P06 | clock ✅×3 | **94** | 单 mutex+cv；owner_thread 递归检测；owner 锁外 loader/Clock；invalidate 不缓存 |
| coding-deepseek | P01 | clock ✅×3 | **93** | thread_local 递归检测（进程级，跨实例边界）；flight 合并完整 |
| coding-glm | P02 | clock ✅×3 | **92** | flight 合并完整；LRU O(n) 扫描（性能非正确性） |
| coding-qwen | P05 | clock ✅×3 | **90** | 通过合并/重入/TTL/invalidate |
| coding-minimax | P04 | clock ✅×3 | **74** | 公开检查过但矩阵缺口：invalidate 不置在途标志致结果回写、无递归检测致同 key 自等死锁 |
| coding-kimi | P03 | clock ⏰×3 | **47** | 确定性死锁（state->now() 在 state->mutex 锁内调用，Clock 重入 invalidate） |

### Q3 routing-config（Refactor/Design）

| 参评对象 | 匿名 | 公开检查 | 题分 | 主要依据 |
|----------|------|----------|------|----------|
| coding-qwen-preview | P06 | observer ✅×3 | **93** | per-instance TLS pin + 局部 new_snapshot 通知（无 published_ 锁外读竞争） |
| coding-glm | P02 | observer ✅×3 | **92** | per-instance pin（实例指针为 key）+ 锁外通知 |
| coding-deepseek | P01 | observer ✅×3 | **84** | 成员 pinned_ per-thread pin；但 reload 通知行52 锁外读 published_（shared_ptr 数据竞争）+ subscribe 锁内移动 Observer |
| coding-minimax | P04 | observer ✅×3 | **70** | 无 TLS pin（靠 16 条 history 保活，违反第3条） |
| coding-kimi | P03 | observer ⏰×3 | **46** | 确定性死锁（裸 Observer 在 reload 锁内拷贝→ReenterOnCopy） |
| coding-qwen | P05 | observer ⏰×3 | **44** | 死锁 + 裸指针+只增不缩 history_（违反第7条）+ ANSWER 编造代码中不存在的 thread_pins |

## 关键差异

- **第一梯队（qwen-preview 281 / glm 279，差距仅 2 分）**：三项并发公开检查全部稳定通过。qwen-preview 的 Q3 reload 用局部 `new_snapshot` 通知、glm 的 Q1 用 `shared_ptr<Callback>` + `max_original_id` 批次冻结，分别最贴近参考设计。两者可视为本轮并列领先，分差未达「明显领先」阈值（平均分差≥15），结论为**存在微弱优势，建议补测确认稳定性**。
- **deepseek（265）**：公开检查全过，但源码有两处真实并发隐患——Q1 `drain` 锁内拷贝含 Callback 的 `Subscription` 向量、Q3 reload 通知在锁外读成员 `shared_ptr published_`（数据竞争）。均未被公开测试覆盖故未触发，但属验收矩阵缺口，拉低正确性/C++ 质量。
- **第二梯队（minimax 187 / qwen 182）**：minimax 在 Q1 死锁且改了公共 API；qwen 在 Q1、Q3 死锁，Q3 还用裸指针 + 只增不缩 history 规避生命周期，ANSWER 编造不存在的 `thread_pins` 字段。
- **第三梯队（kimi 141）**：三题全部确定性死锁。kimi 在「持锁调用用户 callable / 锁内调用用户 Clock」这一最核心的并发原则上全面失守。
- **验证证据**：glm 附带自测 `edge_case_checks.cpp`，deepseek/qwen-preview/qwen 的 ANSWER.md 详细；minimax/kimi 的 ANSWER 偏简或含与代码不符描述。

## 风险与限制

- **样本量**：N=6，单一预设（v1 组合 Bugfix/实现/重构），3 题。不应外推到长上下文（见 v2 报告）或其他题型。
- **编译器**：本机 g++ 9.4.0，C++17 支持充分；6 个模型同一环境，固定。
- **ASAN 紧超时**：Q3 `cross_instance_current_checks` 带 ASAN 在 5s 内偶发 124（约 1/8–1/3），经 no-ASAN 多次验证全过，判为 ASAN 开销+调度，非逻辑死锁；已透明记录，不计为缺陷。
- **盲评严格度差异**：两名评分员在硬封顶区间内尺度不同（严 30-40 vs 松 56，均合法 ≤60），采用平均分缓解；定性结论以客观公开检查锚点为准，不受此影响。
- **时间盒**：推荐 50/70/60 分钟，自动化运行以 7200s 墙钟上限控制，全部在 ~5–13 分钟内完成，未触发超时。

## 下一轮建议

- v1 区分度高（281 vs 141，差 140）：qwen-preview/glm 明显领先第一梯队，kimi 明显落后。
- qwen-preview 与 glm 仅差 2 分，本轮**未拉开显著差距**，建议结合 v2 长上下文报告综合排名后再下定论。
- 对 kimi/qwen/minimax 的死锁根因（持锁调用用户 callable）值得单独归因分析。
