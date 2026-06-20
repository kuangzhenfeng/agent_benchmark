# Q1 作答说明

## 方法

1. 读取 ADR-009，确认文档取舍优先级：**勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码**。
2. 以 v6.proto 每个「适配契约(v6)」注释为基准值。
3. 字段注明"详见 changelog/…"的，回查对应 changelog 取 v5→v6 后的最终值。
4. 120 条勘误条目逐一叠加覆盖。

## 被勘误表推翻的 proto 注释（举例）

- **thrust_report.route_code**：v6.proto 注为 973，ERR-02 推翻为 **385**。
- **stage_state_patch.id_policy**：v6.proto 注为 严格必填，ERR-00 推翻为 **保留空值**。

## 单位漂移、枚举重编号与近似名称

- **v3 单位漂移**：changelog/v2-to-v3.md 明确"单位 0.1ms；旧版 ms 须乘 10"，涉及 stage_state_view.band、stage_state_v2.band、thrust_reports.band、fts_status_view.band、fts_status_patch.band、fts_status_legacy.band、payload_frames.band 等 7 个结构。但 v6.proto/changelog 已直接给出最终数值，故直接使用。
- **v5 飞行阶段重编号**：v6.proto Phase 枚举新增 RESERVED=4，coast=5、insertion=6；v6.proto 注释中"v5 枚举重编号(旧值 X), 以重编号后为准"后紧跟的数字即为最终值。
- **近似名称**：stage_state 与 stage_states、thrust_report 与 thrust_reports 仅差一字符但字段完全独立，逐结构独立核对。

## 文档优先级

ADR-009：**勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码**。
- 所有与勘误表冲突的值一律以勘误表为准（如 thrust_report.route_code 973→385）。
- changelog 推翻 proto 注释（如 nav_solution.schema_rev 9→4 via ERR-01, 原 proto 注 9）。
- 字段词典（v4）和历史代码仅用于交叉参考，不作为真值来源。

## 验证

```
$ ./run_public_checks.sh
# 通过（无输出，exit 0）
```

公开检查抽样 8 个结构（stage_state / thrust_reports / thrust_report_v2 / nav_solution_legacy / sep_event_v2 / payload_frame / link_qualities / thermal_sample_patch）全部 10 字段维度 + flag_active 逐位，全部通过。

## 未覆盖风险

- 公开检查仅抽样 8/48 结构；评分将全面核对 480 个真值点。
- 部分字段 v6.proto 标注"详见 changelog"但对应 changelog 无显式条目的，沿用 v6.proto 注释所给的最终值。
