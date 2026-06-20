# Agent Benchmark 评测报告 · 20260620-1936-cpp-benchmark-v2

> 预设 `cpp17-advanced-v2`（长上下文协议实现 + 协议适配，C++17）。6 个 opencode coding 模型经 `xag` 接入，2 题。

## 匿名映射（仅本报告解盲用）

| 匿名 | 参评对象 | 匿名 | 参评对象 |
|------|----------|------|----------|
| P01 | coding-deepseek | P04 | coding-minimax |
| P02 | coding-glm | P05 | coding-qwen |
| P03 | coding-kimi | P06 | coding-qwen-preview |

## 总结

本轮 6 个 opencode coding 模型在 v2 长上下文协议任务上**两极分化严重**：

- **第一梯队：coding-deepseek（P01）与 coding-glm（P02）并列满分（200/200）**，两题 ground truth 全对（Q1 480/480、Q2 528/528），src 重写为真值，公开检查全过，ANSWER 完整。
- **coding-qwen-preview（P06，199）紧随**，Q2 满分、Q1 仅 8 处字段漏改（98.3%）。
- **coding-qwen（P05，123）严重偏科**：Q2 完美迁移（528/528），但 Q1 留 starter 未作答（公开检查断言失败）→ Q1 硬封顶。
- **末位：coding-minimax（P04）与 coding-kimi（P03）并列（47）**，两题 src 均与 starter 字节一致（未作答），ANSWER 为空模板。其中 **kimi 因上下文超长被服务端拒绝（400，EXIT=1）中途崩溃**，属能力失败而非未尝试。

结论置信度**高**：主 agent 确定性 machine-grade 脚本逐字段比对 ground truth，客观可信；两名独立评分员排名完全一致。

## 盲评流程

- 匿名 ID：P01..P06（按 slug 字母序，仅主 agent 解盲用）
- 评分员：两个独立 scorer subagent（A、B），均只读 `blind-package/` + `scorer-reference/`，均确认未读取身份信息
- 主 agent 额外用确定性 `machine_grade.py` 对每提交编译 + dump 派生真值，与 ground truth JSON 逐字段比对，作为正确性客观依据（`scoring/machine-grade.json`，已匿名化供 scorer 引用）
- 两评分员排名完全一致；唯一分差项（P03/P04 Q2，A=30/B=11）均落在「未提交核心代码 ≤30」硬封顶区间合法，取平均（20）
- 取两评分员平均分为最终题分

## 总分（200 满 = Q1 100 + Q2 100，两名评分员平均）

| 排名 | 参评对象 | 匿名 | Q1 | Q2 | 总分 | 结论 |
|------|----------|------|----|----|------|------|
| 1 | opencode + coding-deepseek | P01 | 100 | 100 | **200** | 两题 ground truth 全对；src 重写真值，公开检查全过，ANSWER 完整 |
| 1 | opencode + coding-glm | P02 | 100 | 100 | **200** | 两题全对；ANSWER 含逐字段来源层 |
| 3 | opencode + coding-qwen-preview | P06 | 99 | 100 | **199** | Q1 漏改 8 处（thrust_report_view.id_policy 等），Q2 满分 |
| 4 | opencode + coding-qwen | P05 | 27 | 96 | **123** | Q2 完美迁移（528/528）；Q1 留 starter 未作答→硬封顶，ANSWER 空 |
| 5 | opencode + coding-minimax | P04 | 27 | 20 | **47** | 两题 src 与 starter 字节一致（未作答），ANSWER 空模板 |
| 5 | opencode + coding-kimi | P03 | 27 | 20 | **47** | 上下文超限(EXIT=1)崩溃；两题未作答，ANSWER 空 |

> 各维度满分：正确性 45 / C++ 质量 20 / 约束 15 / 可维护性 10 / 验证 10（每题 100）。

## 分题评分（两名评分员平均；维度拆分见 `scorer-report-A.md` / `scorer-report-B.md`）

### Q1 long-context-protocol（长上下文实现，480 真值点 = 48 结构 × 10 维）

| 参评对象 | 匿名 | machine-grade | 公开检查 | 题分 | 主要依据 |
|----------|------|---------------|----------|------|----------|
| coding-deepseek | P01 | 480/480 (100%) | ✅ | **100** | v6 规则表全对；按 ADR-009 优先级交叉核对；ANSWER 详 |
| coding-glm | P02 | 480/480 (100%) | ✅ | **100** | 全对；ANSWER 含逐字段来源层 |
| coding-qwen-preview | P06 | 472/480 (98.3%) | ✅ | **99** | 漏改 8 处：thrust_report_view.id_policy、nav_solution_patch.replay_scope、nav_solution_v2.stamp_required、fts_status.schema_rev、fts_status_legacy 等 |
| coding-qwen | P05 | 259/480 (54.0%) | ❌134 | **27** | src 与 starter 字节一致（未作答），留错误直觉值；ANSWER 空模板 |
| coding-minimax | P04 | 259/480 (54.0%) | ❌134 | **27** | 同上，未作答；ANSWER 空 |
| coding-kimi | P03 | 259/480 (54.0%) | ❌134 | **27** | 上下文超限崩溃；未作答；ANSWER 空 |

### Q2 protocol-adaptation（协议适配，528 真值点 = 48 结构 × 11 维）

| 参评对象 | 匿名 | machine-grade | 公开检查 | 题分 | 主要依据 |
|----------|------|---------------|----------|------|----------|
| coding-deepseek | P01 | 528/528 (100%) | ✅ | **100** | 系统 B 真值全对，无机械复制系统 A |
| coding-glm | P02 | 528/528 (100%) | ✅ | **100** | 全对；A/B 枚举对照 |
| coding-qwen-preview | P06 | 528/528 (100%) | ✅ | **100** | 全对 |
| coding-qwen | P05 | 528/528 (100%) | ✅ | **96** | 完美迁移；但 ANSWER 为空模板扣验证 7 分 |
| coding-minimax | P04 | 129/528 (24.4%) | ❌134 | **20** | src 与 starter 一致（系统 A 迁移前值），未作答 |
| coding-kimi | P03 | 129/528 (24.4%) | ❌134 | **20** | 同上；上下文超限崩溃 |

## 关键差异

- **deepseek 与 glm 并列满分（200）**：两题 ground truth 全对，是本轮仅有的两个完整通过长上下文 + 协议适配的对象。两者 src md5 不同（独立实现），无依据区分高下，**本轮未拉开显著差距**（差 0 分）。
- **qwen-preview（199）仅差 1 分**：Q1 漏改 8 处字段（集中在 id_policy/replay_scope/stamp_required/schema_rev 的跨来源取舍边界），属个别幻觉陷阱失守，整体能力与第一梯队接近。
- **qwen（123）严重偏科**：Q2 完美（横向迁移能力强），但 Q1 完全未作答（长上下文纵向记忆任务放弃），两题能力表现割裂。值得注意的是 qwen 在 v1 同样是长上下文/并发偏弱（Q1/Q3 死锁），但 v2 的 Q2 横向迁移反而最强。
- **minimax 与 kimi 并列末位（47）**：两题均未作答（src = starter）。minimax 的 ANSWER 是空模板；**kimi 则是真实的能力失败**——它在反复用内联 python 脚本碎片化解析 `系统B/status.proto` 时，对话上下文累积超过 262144 token 上限，被服务端 400 拒绝、EXIT=1 崩溃，全程 306 步、68 分钟无有效产出。这直接印证了 v2 设计意图：长上下文任务会压垮上下文管理能力弱的模型。
- **验证证据（ANSWER.md）**：deepseek/glm/qwen-preview 的 ANSWER 详细（逐字段来源层、陷阱处理）；qwen 的 Q2 满分但 ANSWER 空（扣验证分）；minimax/kimi 全空。

## 风险与限制

- **样本量**：N=6，单一预设（v2 长上下文/协议适配），2 题。不应外推到 bugfix、并发、重构（见 v1 报告）。
- **未完成项**：minimax/kimi 两题均未提交核心代码（硬封顶 ≤30），其"末位"结论成立但部分反映的是「未完成」而非「答错」；kimi 的未完成根因（上下文超限）已单独归因。
- **54.0%/24.4% 非"真实正确率"**：machine-grade 对未作答（src=starter）的提交仍会跑 starter 默认值与真值的偶然重合，这两个比例仅说明 starter 离真值有多远，不作为能力依据；能力依据是「是否独立作答 + 已作答部分的正确率」。
- **编译器**：本机 g++ 9.4.0，C++17 支持充分；6 个模型同一环境，固定。
- **时间盒**：推荐 300 分钟/题，实际 deepseek/glm/qwen-preview 在 ~10-36 分钟完成，kimi 68 分钟后崩溃，均远低于 300 分钟上限。

## 与 v1 综合对照（跨预设，仅供讨论，非合并排名）

| 模型 | v1 总分/300 | v2 总分/200 | 综合 |
|------|-------------|-------------|------|
| coding-deepseek | 265（v1 第3） | 200（v2 并列第1） | v2 长上下文最强，v1 并发有锁边界缺口 |
| coding-glm | 279（v1 第2） | 200（v2 并列第1） | 两个预设均前列，最均衡 |
| coding-qwen-preview | 281（v1 第1） | 199（v2 第3） | v1 领先，v2 仅差 1 分，综合最强 |
| coding-qwen | 182（v1 第5） | 123（v2 第4） | v1 并发偏弱，v2 横向迁移强但纵向放弃 |
| coding-minimax | 187（v1 第4） | 47（v2 末位） | v1 中等，v2 长上下文放弃 |
| coding-kimi | 141（v1 末位） | 47（v2 末位） | 两个预设均末位 |

**综合结论**：qwen-preview（v1 第1 + v2 第3，仅差 1 分）与 glm（v1 第2 + v2 并列第1）在两个预设上最稳健均衡，为本轮综合最优；deepseek 在 v2 长上下文上最强但 v1 并发有缺口。kimi 两个预设均末位，长上下文管理是明确短板。

## 下一轮建议

- v2 区分度极高（200 vs 47）：deepseek/glm/qwen-preview 明显领先长上下文能力，kimi/minimax 明显放弃或失败。
- deepseek 与 glm v2 满分但完全未区分（差 0），若要细化长上下文能力排序，可提高陷阱密度（更多 errata 推翻/近名变体）或要求逐字段 provenance 必填。
- 建议关注 qwen 的偏科（纵向长上下文放弃 vs 横向迁移满分）与 kimi 的上下文超限崩溃，这两个现象在工程选型上有实际参考价值。
