# benchmark-v2 私有评分参考

仅在全部参评对象停止作答后复制给匿名评分员。不得进入公共题源、参评目录或作答前交付物。

本文件是公开协议的语义校准与验收矩阵，不是唯一源码模板。只要行为等价、资源安全且遵守题面，必须接受不同的数据结构与设计。私有 ground truth JSON（`q*/_private/*_truth.json`）与本文件配套，由生成器确定性产出，二者须一致。

本预设的协议为全新虚构的航空航天测控物模型协议 `range.tc` / `orbit.tc`（proto3，仿物模型工程组织但内容与 tsl 无关）。

## Q1：range.tc v6 长上下文校验器

- 任务是实现最新版 v6 的规则表。v6 真值散落在 `corpus/` 多份文档，须按 `adr/ADR-009.md` 的优先级取舍：**勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码**。
- 48 个绑定结构 × 10 字段 = 480 个真值点，全部见 `q01-long-context-protocol/_private/q01_truth.json`。
- 公开检查只抽样 8 个结构 + `flag_active`；评分须逐字段核对全部 480 点。
- 真值点陷阱分布与可解性（生成器审计保证 100% 可从 corpus 推导）：

| 陷阱类型 | 真值来源 | 诱饵（错误来源） |
|----------|----------|------------------|
| errata_overrides (~25%) | errata/ERR-NN.md | v6.proto 注释（被推翻） |
| v6_proto_full (~25%) | v6.proto 注释 | 无 |
| changelog_lead (~19%) | changelog vX-to-v6.md | v6.proto 仅指向 |
| historical_bug (~11%) | v6.proto 注释 | legacy-code validator_v2/v4（已知 bug） |
| near_name (~9%) | v6.proto 注释（注意变体差异） | 相邻 _legacy/_v2 变体值 |
| enum_renumber (~5%) | changelog v4-to-v5（重编号后） | 旧枚举编号 |
| dict_stale (~5%) | changelog v4-to-v5（重新启用） | dict 字段词典（标"已废弃"） |
| unit_drift (~1%) | changelog v2-to-v3（0.1ms） | 旧单位 ms |

- starter 填的是"未读全语料的错误直觉值"，约 45% 真值点错误，必须由模型校正。

| 审查点 | 正确判断 |
|--------|----------|
| 仅据 v6.proto 或历史代码/词典单一来源填值 | 违反优先级，触发硬性封顶 |
| 被勘误推翻的字段仍用 proto 值 | 错误 |
| 按历史测试 golden_v3 的旧值 | 错误（v3 已过时） |
| 近似变体名称混用 | 错误 |
| 修改 corpus / 公开 API / 测试 | 硬性封顶 |

## Q2：系统 A 向系统 B 适配

- 只能修改 `src/protocol_adapter.cpp`。公开头文件、测试、协议工件、非绑定结构均不可修改。
- 系统A（range.tc）只提供迁移动机与定位；`q02-protocol-adaptation/_private/q02_truth.json` 的 `system_B` 是 48 个实现绑定结构的最终真值。
- 系统 A 与系统 B 的飞行阶段编号不同（B 在 4 号位插入 RESERVED，coast: A=4/B=5，insertion: A=5/B=6），不得照搬。
- 派生函数必须由规则表驱动：`accepts`（仅 strict 拒绝空标识，stamp/ordinal 独立）、`replayable`（≠none）、`flag_active`（逐位）、`phase_rank`、`branch_of`、`within_band`。
- 公开检查抽样 8 个结构；评分核对全部 48 项 × 10 维度。

| 审查点 | 正确判断 |
|--------|----------|
| 把系统 A 值机械复制到系统 B | 违反目标协议 |
| 用系统 A 阶段编号填系统 B phase_value | 错误 |
| 为非绑定上下文结构新增规则 | 违反范围约束 |
| 模式位整体复制 / 选错 oneof 分支 | 错误 |

## 通用评分使用规则

- 先以 QUESTION.md 与结构化协议/corpus 为准；本文件与 ground truth JSON 只帮助识别看似合理但违约的实现。
- 行为等价、约束遵循的实现必须接受；不得以"现实系统通常会有"的额外策略（负载哈希、单位额外换算、全局去重等）扣分。
- corpus 与协议工件均为公开题源；评分不依赖未公开的私人隐藏条件。
- 本参考不得出现在公开题源、参评目录、作答前交付或面向参评者的最终报告中。
