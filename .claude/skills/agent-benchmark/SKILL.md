---
name: agent-benchmark
description: 当需要评测多个 AI Coding Agent、模型组合或接入配置在 C++ 编码任务上的效果时使用；先询问参评 Agent/模型，再生成隔离的 C++ benchmark 试题目录，等待用户让各 Agent 作答后，由主 agent 创建匿名评分包并调用 scorer subagent 盲评，最后解盲汇总报告。
---

# Agent Benchmark（编码 Agent 基准评测）

## 概述

用“参评对象确认 → 同题试题包生成 → 独立目录作答 → 匿名 subagent 盲评 → 解盲汇总报告”的流程评测 AI Coding Agent，避免评分方知道参评身份导致偏差。

## 核心原则

| 原则 | 要求 |
|------|------|
| 先问对象 | 用户未提供参评 Agent + 模型前，只询问参评对象，不直接出题 |
| 同题同源 | 所有参评对象拿到完全相同的 C++ 题目与公开要求 |
| 目录隔离 | 每个参评对象只在自己的 `agents/<slug>/` 下作答 |
| 不泄答案 | 生成阶段不向题源或参评目录写参考答案、隐藏测试或评分结论；私有参考解答仅在作答结束后提供给匿名 scorer |
| 匿名盲评 | 评分必须由 scorer subagent 基于匿名包完成，不能看到 Agent 或模型名称 |
| 证据评分 | scorer subagent 基于源码、题面、验证记录和统一 rubric 评分 |
| 克制结论 | 只对本轮题型和样本范围下结论，不外推到所有任务 |

## 公平性控制

生成和评分前主动检查会影响公平性的因素，能固定就固定，不能固定就记录为限制。

| 风险 | 影响 | 控制方式 |
|------|------|----------|
| 评分者知道参评身份 | 对熟悉的 Agent/模型产生期待偏差 | 主 agent 只做组织和解盲；scorer subagent 只看匿名 `P01/P02` |
| 题目或答案泄漏 | 后作答者可能受益 | 同一时间冻结题目；私有参考解答不进入题源/作答目录，且只在全部作答结束后提供给 scorer；作答目录互相隔离 |
| 执行顺序差异 | 后运行者可能继承前者产物或上下文 | 每个参评对象使用独立目录；不要在同一对话里连续透露结果 |
| 工具权限不同 | 搜索、编译、联网能力影响成绩 | 在 `participants.md` 记录联网、文件读写、命令执行、测试权限 |
| 时间盒不同 | 花费时间越长越可能改好 | 对每题设置相同时间盒；超时按未完成项记录 |
| 提示词不同 | prompt 质量而非 Agent 能力拉开差距 | 给所有参评对象使用同一 `agents/<slug>/README.md` 和同一题面 |
| 环境不同 | 编译器、标准库、系统差异影响测试 | 题目自包含；记录可用编译器和测试方式；评分时注明未固定环境 |
| 任务覆盖偏窄 | 单类题目不能代表整体能力 | 默认覆盖 bugfix、implementation、refactor/design |
| 题目太主观 | 分数受评分者偏好影响 | 每题写清可判定验收点；按统一 rubric 给分并引用代码事实 |
| 匿名信息残留 | `ANSWER.md`、路径、日志泄露身份 | 创建盲评分包时把 Agent/模型名、原目录 slug、对话来源改为 `[REDACTED]` |

## 执行流程

### 1. 询问参评对象

第一句话必须询问本次参与 benchmark 的 Agent 与模型组合。不要一次问太多问题；题量、难度、时间盒可以让用户顺带提供。

```text
请告诉我本次参与 benchmark 的 Agent + 模型组合，例如：
- claude code + qwen3.7 max
- opencode + glm-5.2

如果你有期望题量、难度或时间盒，也一起告诉我；没有的话我使用 `cpp17-advanced-v1` 的 3 道 C++17 预设题（50 / 70 / 60 分钟）。
```

收到回复后整理参评表：

| 字段 | 规则 |
|------|------|
| 参评对象 | 原样保留用户写法，如 `claude code + qwen3.7 max` |
| slug | 转成小写短横线目录名，如 `claude-code-qwen3-7-max` |
| 题量 | 用户指定优先；未指定时使用 3 题预设套题 |
| 难度 | 用户指定优先；默认 `cpp17-advanced-v1`（高级工程实践） |
| 时间盒 | 用户指定优先；默认 Q1 50 分钟、Q2 70 分钟、Q3 60 分钟 |

若只有一个参评对象，可以生成单对象能力测评，但最终报告不能写成 A/B 差异。

### 2. 选择或设计 C++ 试题

默认使用仓库内版本化预设 `cpp17-advanced-v1`，路径为：

```text
.claude/skills/agent-benchmark/presets/cpp17-advanced-v1/
```

不要在默认流程中临时改题、替换单题或根据参评对象调整题面。预设冻结后，同一轮的所有参评对象必须从同一份预设副本作答。用户明确指定预设名（例如 `cpp17-advanced-v2`）时，使用该预设目录与对应私有评分参考；这不是自定义出题。只有用户明确要求预设之外的题量、题型、领域或难度时，才设计新题；此时要在本轮 `README.md` 记录偏离的原因，不能仍标为该预设版本。

`cpp17-advanced-v1` 固定组合：

| 题号 | 类型 | 推荐耗时 | 评测重点 |
|------|------|----------|----------|
| Q1 `subscription-hub` | 组合 Bugfix | 50 分钟 | 所有权、回调重入、异常安全、32 位时钟回绕、并发批次语义 |
| Q2 `coalescing-cache` | Implementation | 70 分钟 | 并发状态机、请求合并、TTL、失效竞态、LRU、递归加载 |
| Q3 `routing-config` | Refactor/Design | 60 分钟 | 快照发布、兼容 API 生命周期、观察者隔离、原子 reload、最长前缀匹配 |

可选预设 `cpp17-advanced-v2` 固定组合：

| 题号 | 类型 | 推荐耗时 | 评测重点 |
|------|------|----------|----------|
| Q1 `settlement-journal` | 组合缺陷修复 | 65 分钟 | 1,000 行 C++ 结构体业务协议、优先级、作用域幂等、文本所有权、审计重入 |
| Q2 `benefit-allocation` | 实现 | 90 分钟 | 3,000 行 JSON 业务协议、单条/批次回放、临时账本、原子提交、回调隔离 |
| Q3 `protocol-adaptation` | 协议适配 | 210 分钟 | 协议一 Protobuf 与协议二 JSON 各 5,000 行、近似结构逐字段迁移、范围控制 |

`cpp17-advanced-v2` 专门评测长篇业务协议下的闭世界实现和协议适配能力。业务协议本体使用 C++ 结构体、JSON、Protobuf，不使用标记文本文档协议附录；所有组合边界必须公开，不能用未说明的私人“隐藏坑”评分。它提高了按经验臆测业务规则或机械复制近似结构的风险，但不能保证任何特定模型一定失败或通过；结论必须来自本轮盲评。

这些题目通过多个相互影响的验收条件提高排查和实现深度，目标是让强模型也必须经历阅读、设计、实现和验证的多轮工作。不得宣称某个题目能保证所有当前或未来模型都无法单轮完成；模型能力需以实际 benchmark 结果验证。

每题必须包含：

| 文件/内容 | 要求 |
|-----------|------|
| `QUESTION.md` | 目标、背景、约束、验收标准、提交要求 |
| starter code | 小而完整，代码量足够暴露问题但不靠样板拉开差距 |
| public checks | 可公开的验证建议、输入输出样例或测试思路 |
| `ANSWER.md` 模板 | 让参评 Agent 填写修改说明、验证方式、风险 |

默认预设和自定义题目都必须满足：

| 约束 | 要求 |
|------|------|
| 不写答案 | 不在 benchmark 目录保存参考实现或标准答案 |
| 不做脑筋急转弯 | 评测工程编码能力，不考隐晦谜题 |
| 不依赖网络 | 参评 Agent 不应需要联网查资料 |
| 不依赖私有环境 | 题目不能要求特殊 SDK、业务仓库或外部服务 |
| 可静态评分 | 即使不运行测试，也能通过源码和题面判断主要质量 |
| 难点可解释 | 每题的预期拉分点要能在评分报告中说明 |
| 组合复杂度 | Bugfix 题至少包含 3 个会相互影响的缺陷维度，覆盖所有权/生命周期、并发/重入、异常安全、边界算术或容器失效中的至少 3 类 |
| 多层验收 | 公开检查只覆盖基础路径；题面必须列出可由源码与验证证据判定的组合语义，不能以未说明的谜题式隐藏条件评分 |
| 可复验 | 每题提供 starter 和公开检查，并在题面记录 starter 的预期状态 |

### 3. 创建 benchmark 目录

在 `benchmark/<run-id>/` 下创建本轮评测。`<run-id>` 使用日期时间和简短主题，例如 `20260619-1430-cpp-benchmark`。

若使用预设（默认 v1 或用户明确指定的预设），按以下顺序创建题目，不能手写一个看似相同但内容漂移的副本：

1. 将所选预设的全部 qXX 题目目录 **完整复制**到 `questions/`。v1 是三个目录；v2 是 `q01-settlement-journal`、`q02-benefit-allocation`、`q03-protocol-adaptation`。
2. 从刚生成的 `questions/` 完整复制每题到每个 `agents/<slug>/`，包括 `QUESTION.md`、`ANSWER.md`、`include/`、`src/`、`tests/` 与 `run_public_checks.sh`。
3. 在本轮 `README.md` 和 `participants.md` 写明实际 `preset: <name>`、每题时间盒和 C++17 编译器版本。

复制而非符号链接，保证每个作答目录可独立修改且题源在运行结束后可复现。复制后比较各 `agents/<slug>/qXX-*` 与 `questions/qXX-*` 的初始内容；任何差异都应视为生成失败并重新复制。严禁复制 `.claude/skills/agent-benchmark/scoring-reference/`、任何参考实现或私有验收材料到本轮目录；这些材料只在第 5 步创建 scorer 专用副本。

目录结构：

```text
benchmark/<run-id>/
├── README.md
├── participants.md
├── questions/
│   ├── q01-<short-name>/
│   │   ├── QUESTION.md
│   │   ├── ANSWER.md
│   │   ├── include/
│   │   ├── src/
│   │   └── tests/
│   ├── q02-<short-name>/
│   └── q03-<short-name>/
├── agents/
│   ├── <agent-model-slug>/
│   │   ├── README.md
│   │   ├── q01-<short-name>/
│   │   ├── q02-<short-name>/
│   │   └── q03-<short-name>/
│   └── <agent-model-slug>/
├── submissions/
│   ├── <agent-model-slug>/
│   └── <agent-model-slug>/
└── scoring/
    ├── blind-package/
    │   ├── README.md
    │   ├── P01/
    │   └── P02/
    ├── scorer-reference/       # 作答结束后才创建，仅供 scorer
    │   └── cpp17-advanced-v1.md
    ├── mapping.private.md
    ├── redaction-log.md
    └── scorer-report.md
```

目录职责：

| 路径 | 用途 |
|------|------|
| `README.md` | 本轮评测入口，说明如何分发、作答、完成后如何通知评分 |
| `participants.md` | 记录参评对象、题量、时间盒、固定变量 |
| `questions/` | 公共题源，用于确认所有 Agent 同题 |
| `agents/<slug>/` | 给单个参评对象使用的独立作答目录 |
| `submissions/<slug>/` | 用户可选的结果归档目录 |
| `scoring/blind-package/` | 用户完成后由主 agent 创建，给 scorer subagent 盲评 |
| `scoring/scorer-reference/` | 全部作答结束后才创建的私有参考解答副本；只允许匿名 scorer 读取，绝不分发给参评对象 |
| `scoring/mapping.private.md` | 匿名 ID 到参评对象的映射，只给主 agent 使用，不给 scorer subagent |
| `scoring/redaction-log.md` | 记录脱敏改动，只写脱敏位置和类型，不写参评身份 |
| `scoring/scorer-report.md` | scorer subagent 输出的匿名评分报告 |

本轮 `README.md` 还必须按所选预设声明公开检查的初始状态。v1：Q1、Q3 的基础路径可通过，但完整公开套件含已文档化的边界检查而预期失败；Q2 因 starter 未实现而预期失败。v2：Q1 可编译但因公开业务协议边界缺陷失败；Q2 未实现；Q3 的规则表仍是协议适配前值。不得把该状态解释为对某个参评对象的预评分。

每个 `agents/<slug>/README.md` 必须写清楚：

```markdown
# Agent Benchmark 作答说明

你只允许在当前目录内答题。

禁止：
- 读取其他 `agents/<slug>` 目录
- 读取或参考 `submissions/` 中其他参评对象结果
- 修改 benchmark 公共题源 `questions/`

要求：
- 每题在对应 `qXX-*` 目录内修改或新增文件
- 填写每题 `ANSWER.md`
- 记录你执行过的验证方式；如未验证，说明原因
- 完成后停止，不要替其他 Agent 评分
```

生成完成后停止推进，告诉用户分别把待评测 Agent 指向各自目录作答。

### 4. 等待用户提交结果

生成目录后不要替参评 Agent 作答，也不要提前评分。只给用户交付说明：

```markdown
已生成 benchmark：

- 总入口：[README.md]
- 参评对象：[participants.md]
- [Agent + 模型] 作答目录：[agents/<slug>/]

请分别让待评测 Agent 在各自目录内完成答题。全部完成后告诉我“已完成，结果在 <run-id>”，我再读取结果并评分。
```

用户后续说“已完成”“开始评分”“结果在某目录”时，进入评分阶段。

### 5. 创建匿名评分包并调用 scorer subagent

用户完成后，主 agent 先读取提交结果，但不能直接评分。主 agent 的职责是构造匿名评分包、调用 scorer subagent、保存匿名评分报告。

主 agent 必须读取：

- `participants.md`
- 每题公共 `questions/qXX-*/QUESTION.md`
- 每个参评对象的 `agents/<slug>/qXX-*` 或 `submissions/<slug>/`
- 每题 `ANSWER.md`、源码变更、测试或验证记录
- 对应预设的 `.claude/skills/agent-benchmark/scoring-reference/<preset>.md`

所有参评对象完成并确认停止作答后，主 agent 才可将私有评分参考解答复制到 `scoring/scorer-reference/<preset>.md`。复制前后记录文件 SHA-256 到 `scoring/reference-integrity.md`。该副本不得包含参评身份、评分结论或任何参评提交内容；若本轮不是预设题，主 agent 必须在生成阶段同步创建对应私有评分参考解答，否则不能声称评分依据完整。

匿名包创建规则：

| 步骤 | 要求 |
|------|------|
| 分配匿名 ID | 用 `P01`、`P02`... 代替参评对象；顺序不要在 scorer prompt 中解释 |
| 写映射文件 | 在 `scoring/mapping.private.md` 记录 `Pxx -> Agent + 模型`，不要传给 scorer subagent |
| 复制题面 | 每个 `Pxx` 下放同一份 `QUESTION.md` 和提交结果，路径不含原 slug |
| 清理身份信息 | 将 Agent 名、模型名、原目录名、对话来源替换为 `[REDACTED]`；不要改技术内容 |
| 保持完整性 | 不选择性摘录好/坏证据；除身份脱敏外，完整保留提交源码和验证材料 |
| 记录脱敏 | 在 `scoring/redaction-log.md` 记录哪些文件做过身份脱敏 |
| 保留证据 | 保留源码、测试、验证输出、ANSWER 的技术说明 |
| 写评分说明 | 在 `scoring/blind-package/README.md` 写 rubric、扣分规则和输出模板，不写参评身份 |
| 放入参考解答 | 仅在全部作答完成后，将不含身份的参考解答放入 `scoring/scorer-reference/`；不得放进 `blind-package/Pxx` 或任何参评目录 |

若提交内容大量包含无法安全清理的身份信息，先记录为公平性风险，再尽量只向 scorer subagent 提供源码 diff、题面和必要验证记录。若代码风格本身可能暴露身份，只能在最终报告中列为残余风险，不要伪装成完全双盲。

评分必须交给 scorer subagent。若当前工具列表没有直接可用的 subagent 能力，先通过 `tool_search` 搜索 multi-agent/subagent 工具；若仍不可用，必须告诉用户“无法保证盲评”，不要由主 agent 冒充盲评。

scorer subagent prompt 模板：

```text
你是 C++ benchmark 的盲评评分员。你只能读取以下两个目录：

[scoring/blind-package 绝对路径]
[scoring/scorer-reference 绝对路径]

你不知道、也不需要知道 P01/P02 分别对应哪个 Agent 或模型。不要尝试推断身份，不要读取上述两个目录之外的 mapping、participants、agents、submissions 目录。

私有参考解答用于校准公开语义和识别伪修复，不是唯一实现模板。请按 blind-package/README.md 中的 rubric 对每个匿名参评对象逐题评分，接受任何行为等价的实现。评分必须引用题面要求、源码事实和验证证据。只使用 P01/P02 这类匿名 ID。
```

每题 100 分：

| 维度 | 分值 | 评分标准 |
|------|------|----------|
| 正确性 | 45 | 是否满足题面要求，核心行为和边界条件是否正确 |
| C++ 质量 | 20 | 是否合理使用 C++17、RAII、const、类型边界、错误处理 |
| 约束遵循 | 15 | 是否遵守目录隔离、题面限制、提交格式和禁止事项 |
| 可维护性 | 10 | 是否简洁清晰、职责明确、无过度封装或无关重写 |
| 验证证据 | 10 | 是否提供可信测试、命令输出、推理或人工验证说明 |

硬性扣分：

| 情况 | 处理 |
|------|------|
| 未提交核心代码 | 该题总分不超过 30 |
| 编造测试、命令或不存在的文件 | 该题总分不超过 40 |
| 读取或引用其他参评对象结果 | 该题总分不超过 50 |
| 违反 `QUESTION.md` 中明确标记为“硬性封顶条件”的禁止事项 | 该题总分不超过 60 |
| 大量无关重写导致无法审查 | 可维护性为 0，正确性按可验证部分给分 |
| 破坏题目结构导致无法判断 | 该题总分不超过 50 |

高敏感或分差很小的评测，优先创建两个独立 scorer subagent 分别盲评。若两者总分差异超过 8 分或单题差异超过 12 分，主 agent 只基于匿名 ID 对差异项做复核说明，不在复核前解盲。

### 6. 解盲并输出评测报告

收到 scorer subagent 的匿名报告后，主 agent 才读取 `scoring/mapping.private.md` 做解盲汇总。主 agent 不重新打分，只做：

- 将匿名 ID 映射回参评对象
- 汇总总分、排名、关键差异
- 记录公平性限制和 scorer subagent 的评分证据
- 若发现 scorer subagent 明显读错题面或漏读提交，先用匿名 ID 要求 scorer subagent 修正，再解盲

在 `benchmark/<run-id>/evaluation.md` 写入最终报告，并在对话中摘要结论。

最终报告模板：

```markdown
# Agent Benchmark 评测报告

## 总结

[一句话说明本轮谁领先、领先在哪类任务、置信度如何。]

## 盲评流程

- 匿名 ID：[P01/P02/...]
- 评分员：scorer subagent
- scorer 是否看到 Agent/模型身份：否
- 未完全匿名的信息：[如有]

## 总分

| 排名 | 参评对象 | Q1 | Q2 | Q3 | 平均分 | 结论 |
|------|----------|----|----|----|--------|------|
| 1 | [Agent + 模型] | [分] | [分] | [分] | [分] | [结论] |

## 分题评分

### Q1 [题名]

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|------|----------|
| [Agent + 模型] | | | | | | | |

## 关键差异

- [差异 1：基于源码或验证证据]
- [差异 2：基于题面要求和提交产物]

## 风险与限制

- 样本量：[N]
- 未运行或无法确认的测试：[说明]
- 不应外推的范围：[说明]

## 下一轮建议

- [是否增加题量、调整难度、聚焦某类能力]
```

结论阈值：

| 结果 | 写法 |
|------|------|
| 平均分差 >= 15 且多数题胜出 | “本轮明显领先” |
| 平均分差 8~15 | “存在优势，建议补测确认稳定性” |
| 平均分差 < 8 | “本轮未拉开显著差距” |
| 单题大胜但总分接近 | “仅在该任务类型上体现优势” |
| 缺少提交或证据 | “无法科学比较”，并列缺失项 |

## 常见误区

| 误区 | 正确做法 |
|------|----------|
| 用户还没给参评对象就开始出题 | 必须先询问 Agent + 模型组合 |
| 不同 Agent 拿到不同题目 | 所有参评对象使用同一份题源复制出的目录 |
| 把参考答案写进题源或作答目录 | 生成阶段只写题面、starter code 和公开验收；私有参考解答只在全部作答结束后复制到 `scoring/scorer-reference/` |
| 让所有 Agent 在同一目录答题 | 每个参评对象使用独立 `agents/<slug>/` |
| 用户未完成就提前评分 | 等用户明确完成后再评分 |
| 主 agent 直接评分 | 主 agent 只创建匿名包和解盲汇总，评分必须交给 scorer subagent |
| 把参评身份传给 scorer subagent | scorer subagent 只能看到 `P01/P02` 匿名 ID |
| 匿名包里残留模型名或目录 slug | 将身份信息替换为 `[REDACTED]`，只保留技术内容 |
| 只看 `ANSWER.md` 自述 | scorer subagent 必须读取源码变更和题面，必要时要求验证输出 |
| 分差很小却下强结论 | 使用分差阈值约束结论 |
