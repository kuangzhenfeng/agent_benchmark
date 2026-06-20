# Q1 作答说明

## 修改范围

仅修改了 `src/range_validator.cpp` 中的规则表 `kRules`，将原本的"未读全语料的错误直觉值"按 ADR-009 优先级校正为 v6 真值。

## 数据来源与取舍方法

按 ADR-009 优先级（勘误表 > changelog > v6.proto 注释 > 字段词典 > 历史代码），对所有 48 个结构的 10 个维度逐一交叉核对：

1. **v6.proto 注释**：作为基线，提取各绑定结构的适配契约元数据（route_code、schema_rev、mode_flag 等）
2. **v4→v5、v5→v6 changelog**：对 v6.proto 中标注"详见 changelog"的值进行补充（如 phase_value 重编号、band 变更等）
3. **12 份勘误表（ERR-00 至 ERR-11）**：逐条覆盖 v6.proto 和 changelog 中的错误值

关键勘误覆盖示例：
- ERR-07：`stage_state.route_code` 230→576，`thrust_report.stamp_required` true→false
- ERR-09：多个结构的 schema_rev、ordinal_required、replay_scope 纠正（如 `stage_state.schema_rev` 5→2）
- ERR-11：`stage_state.replay_scope` 不回放→本箭，`sep_event_v2.schema_rev` 1→6
- ERR-02：`thrust_report.route_code` 973→385，`fts_status.band` [110,285]→[308,432]
- ERR-04：`thrust_report.ordinal_required` true→false，`sep_event.band` [98,345]→[464,557]

## 验证方式

运行公开检查：
```
g++ -std=c++17 -Iinclude src/range_validator.cpp tests/public_checks.cpp -o /tmp/q1_check && /tmp/q1_check
```
输出：PASSED

公开检查验证了 8 个抽样结构（stage_state、thrust_reports、thrust_report_v2、nav_solution_legacy、sep_event_v2、payload_frame、link_qualities、thermal_sample_patch）的所有 10 个维度，以及 stage_state 的 flag_active 逐位断言。

## 已知风险

- 部分结构的 mode_flag 值和 band 区间来自 v6.proto 注释，未被勘误表或 changelog 明确覆盖的保持原值
- 评分环节会逐字段核对全部 480 个真值点，可能存在未发现的错误