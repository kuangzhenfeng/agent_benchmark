# Q2：系统 A 变更向系统 B 适配

测控平台已在系统 A（`range.tc`）完成一次结构契约升级，现在需要把这次变更适配到相似但不等同的系统 B（`orbit.tc`）。请**仅修改 `src/protocol_adapter.cpp`**，使其规则表严格匹配系统 B 的 **48 个"实现绑定"结构**的最终真值。

## 公开输入

- `系统A/`：`range.tc` 物模型（`definitions.proto` + 四象限 `status/action/config/event.proto`）。每个结构注释里写明系统 A 完成升级后的真值；非绑定结构只提供上下游业务上下文。
- `系统A/变更映射.json`：系统 A 变更与系统 B 目标结构的映射，说明迁移动机与结构代号定位。
- `系统B/`：`orbit.tc` 目标物模型。**每个实现绑定结构注释中的真值才是目标字段真值**；非绑定结构不得新增为规则项。
- `include/protocol_adapter.hpp`：不可修改的公开 API。
- `src/protocol_adapter.cpp`：仍保存系统 A 值（迁移前）的起始实现。

## 适配范围

只适配系统 B 中"是否实现绑定：是"的 **48 个结构**（8 子系统族 × 6 变体）。它们**散布在 348 个结构**之中，且变体名称具有单/复数、视图、补丁、legacy、v2 等近似形式；每个结构的 10 个字段维度必须独立读取。

系统 B 的字段与 C++ 类型的对应：

| 物模型字段 | 取值 | C++ 值 |
|-----------|------|--------|
| 标识策略 id_policy | 严格必填 | `IdPolicy::strict` |
| 标识策略 id_policy | 保留空值 | `IdPolicy::keep_blank` |
| 标识策略 id_policy | 空则生成 | `IdPolicy::auto_gen` |
| 回放域 replay_scope | 不回放 | `ReplayScope::none` |
| 回放域 replay_scope | 本级 | `ReplayScope::stage` |
| 回放域 replay_scope | 本箭 | `ReplayScope::vehicle` |
| 回放域 replay_scope | 本靶区 | `ReplayScope::range` |
| 载荷分支 branch | 遥测 / 指令 / 配置 / 事件 | `Branch::telemetry/command/config/event` |

派生函数必须继续由规则表驱动：
- `accepts`：仅"严格必填"拒绝空标识；`stamp_required` 与 `ordinal_required` 互相独立。
- `replayable`：仅当回放域不是"不回放"时为真。
- `flag_active`：逐位判断，**不得**整体复制模式位含义。
- `phase_rank`：返回阶段编号——**注意系统 A 与系统 B 的飞行阶段编号不同**，`coast` 在 A 为 4、在 B 为 5；`insertion` 在 A 为 5、在 B 为 6；系统 B 的 4 号位是 `reserved` 占位。
- `branch_of` / `within_band`：返回对应维度真值。

## 约束与验收

- 不得修改公开头文件、测试、系统 A、系统 B 或 `变更映射.json`。
- 不得根据名称、相邻位置、系统 A 结构注释、所属子系统族或常见航天惯例推断系统 B 字段。
- 不得对标识、回放令牌或样本值增加 trim、哈希、全局去重或单位换算。
- 公开检查只抽样 8 个结构与 `flag_active`；评分会检查**全部 48 项**与范围外修改。
- **硬性封顶条件（该题总分不超过 60）**：把系统 A 的值机械复制到系统 B，或按系统 A 的阶段编号填写系统 B 的 `phase_value`，或修改了任一非绑定结构、公开 API、测试或协议工件。

运行方式：

```bash
./run_public_checks.sh
```
