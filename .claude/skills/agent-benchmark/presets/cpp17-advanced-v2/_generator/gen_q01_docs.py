# coding: utf-8
"""
changelog / errata / adr / dict / legacy-code / legacy-tests / index 渲染。

这些文档是长上下文的"信息散落载体"。每个真值点的 trap 类型决定其真值/误导值
出现在哪份文档、以何种措辞, 使模型必须交叉核对。
"""

import os
import random
import model as M
from model import SEED, FAMILIES, all_bound_codes
import q01_evolution as E
from q01_evolution import VERSIONS, PRIORITY_RULE
from gen_q01_corpus import (_cn_id_policy, _cn_replay, _cn_branch,
                            _bits_desc, _comment_line)


# ---------------------------------------------------------------------------
# changelog: v1->v2 ... v5->v6
# ---------------------------------------------------------------------------

def render_changelogs(v6_truth, out_dir):
    d = os.path.join(out_dir, "changelog")
    os.makedirs(d, exist_ok=True)
    pairs = list(zip(VERSIONS[:-1], VERSIONS[1:]))  # v1->v2 ... v5->v6
    # 收集每份 changelog 要承载的真值点(按 lead_to 与 trap)
    for i, (a, b) in enumerate(pairs):
        title = f"{a}-to-{b}"
        lines = []
        lines.append(f"# 变更日志：{a} → {b}\n\n")
        lines.append(f"本日志记录从 {a} 到 {b} 的结构契约变更。每项给出变更后的最终值。\n\n")
        lines.append("## 变更条目\n\n")
        entry = 1
        for code, info in v6_truth.items():
            for field, meta in info["fields"].items():
                # 该值若 lead_to 指向本 changelog, 或属于本日志覆盖的版本演进
                include = False
                text = None
                if meta["trap"] == "changelog_lead" and meta.get("lead_to") == a:
                    include = True
                    text = _value_text(field, meta["value"])
                elif meta["trap"] == "unit_drift" and b == "v3":
                    include = True
                    text = _unit_drift_text(field, meta["value"])
                elif meta["trap"] == "enum_renumber" and b == "v5":
                    include = True
                    text = _value_text(field, meta["value"])
                elif meta["trap"] == "dict_stale" and b == "v5":
                    include = True
                    text = _value_text(field, meta["value"])
                if include:
                    lines.append(
                        f"- **{code}.{field}**：{a} 值为 {_wrong_text(field, meta)}；"
                        f"{b} 起变更为 **{text}**。\n")
                    entry += 1
        # 充实文档体量(填充性变更说明, 不引入新真值点)
        lines.append("\n## 其他结构变更说明\n\n")
        lines.append(_filler_section(a, b, seed=SEED + i))
        with open(os.path.join(d, f"{title}.md"), "w", encoding="utf-8") as f:
            f.write("".join(lines))


def _value_text(field, val):
    if field == "band":
        return f"[{val[0]}, {val[1]}]"
    if field in ("stamp_required", "ordinal_required"):
        return str(val).lower()
    if field == "id_policy":
        return _cn_id_policy(val)
    if field == "replay_scope":
        return _cn_replay(val)
    if field == "branch":
        return _cn_branch(val)
    if field == "mode_flag":
        return f"{val} ({_bits_desc(val)})"
    return str(val)


def _wrong_text(field, meta):
    """陷阱里 proto/词典/历史代码给的'错误值'。"""
    for key in ("wrong_in_proto", "dict_says", "buggy_value", "neighbor", "old_value"):
        if key in meta:
            return _value_text(field, meta[key])
    return _value_text(field, meta["value"])


def _unit_drift_text(field, val):
    if field == "band":
        return f"[{val[0]}, {val[1]}] (单位 0.1ms; 旧版 ms 须乘 10)"
    return _value_text(field, val)


def _filler_section(a, b, seed):
    rng = random.Random(f"{seed}:filler")
    out = []
    for code in all_bound_codes()[:24]:
        n = rng.randint(1, 4)
        for k in range(n):
            desc = rng.choice([
                f"{code} 的上下文字段 f{rng.randint(1,5)} 含义在 {b} 细化为含方向分量",
                f"{code} 的告警阈值在 {b} 调整, 由固定值改为按级序号查表",
                f"{code} 的帧序号回绕处理在 {b} 修复, 不再影响校验结论",
                f"{code} 在 {b} 新增冗余链路统计字段, 不参与适配契约",
                f"{code} 的字段编号重新分配以预留扩展位, 契约语义不变",
            ])
            out.append(f"- {desc}。\n")
    return "".join(out)


# ---------------------------------------------------------------------------
# errata: ERR-00 .. ERR-11
# ---------------------------------------------------------------------------

def render_errata(v6_truth, out_dir):
    d = os.path.join(out_dir, "errata")
    os.makedirs(d, exist_ok=True)
    # 把 errata_overrides 陷阱按 errata_id 分组
    by_errata = {}
    for code, info in v6_truth.items():
        for field, meta in info["fields"].items():
            if meta["trap"] == "errata_overrides":
                eid = meta.get("errata_id", 1)
                by_errata.setdefault(eid, []).append((code, field, meta))
    for eid in range(12):
        lines = []
        lines.append(f"# 勘误 ERR-{eid:02d}\n\n")
        lines.append("本勘误推翻既有文档(v6.proto 注释或 changelog)中的下列值。"
                     "勘误表优先级最高。\n\n")
        items = by_errata.get(eid, [])
        if items:
            lines.append("## 纠正条目\n\n")
            for code, field, meta in items:
                wrong = _wrong_text(field, meta)
                right = _value_text(field, meta["value"])
                lines.append(
                    f"- **{code}.{field}**：原文档记为 {wrong}，"
                    f"应更正为 **{right}**。\n")
        else:
            lines.append("## 纠正条目\n\n- 本条勘误不涉及适配契约字段"
                         "(仅澄清上下文字段语义), 无需据本条调整规则表。\n")
        # 充实
        lines.append("\n## 背景\n\n")
        lines.append(_filler_section("v6", "v6", seed=SEED + 100 + eid))
        with open(os.path.join(d, f"ERR-{eid:02d}.md"), "w", encoding="utf-8") as f:
            f.write("".join(lines))


# ---------------------------------------------------------------------------
# ADR: 含文档优先级(只此一处)
# ---------------------------------------------------------------------------

def render_adr(out_dir):
    d = os.path.join(out_dir, "adr")
    os.makedirs(d, exist_ok=True)
    lines = []
    lines.append("# ADR-009：协议文档取舍优先级\n\n")
    lines.append("## 决策\n\n")
    lines.append("当同一字段的值在多份文档中不一致时，按以下优先级取舍（高 → 低）：\n\n")
    lines.append(f"**{PRIORITY_RULE}**\n\n")
    lines.append("即：勘误表最高，历史代码最低。任何与勘误表冲突的 proto 注释、"
                 "changelog 或字段词典，一律以勘误表为准。\n\n")
    lines.append("## 理由\n\n")
    lines.append("勘误表为变更后的最终事实；changelog 记录演进；proto 注释可能滞后；"
                 "字段词典停留在 v4、且部分条目已标废弃；历史实现含已知缺陷，不可作为真值来源。\n\n")
    lines.append("## 适用范围\n\n")
    lines.append("本优先级适用于全部 48 个实现绑定结构的全部适配字段。"
                 "实现 v6 校验器时必须严格按此取舍。\n")
    with open(os.path.join(d, "ADR-009.md"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


# ---------------------------------------------------------------------------
# 字段词典(v4, 滞后/部分废弃)
# ---------------------------------------------------------------------------

def render_dictionary(v6_truth, out_dir):
    d = os.path.join(out_dir, "dict")
    os.makedirs(d, exist_ok=True)
    lines = []
    lines.append("# 字段词典 range.tc v4\n\n")
    lines.append("> 本词典停留在 v4。部分字段已废弃或被后续 changelog 重新定义。"
                 "本词典**不可**作为 v6 的唯一真值来源。\n\n")
    lines.append("## 词条\n\n")
    for code in all_bound_codes():
        info = v6_truth[code]
        lines.append(f"### {code}\n\n")
        for field in E.FIELDS_ORDER:
            meta = info["fields"][field]
            # dict_stale: 词典给"错误/废弃"值
            if meta["trap"] == "dict_stale":
                wrong = meta.get("dict_says")
                note = meta.get("dict_note", "已废弃")
                lines.append(f"- {field}: {wrong if wrong else _value_text(field, meta['value'])}"
                             f"  *（{note}）*\n")
            else:
                # 词典只覆盖部分字段(随机跳过, 制造"权威但不全")
                rng = random.Random(f"{SEED}:dict:{code}:{field}")
                if rng.random() < 0.5:
                    lines.append(f"- {field}: {_value_text(field, meta['value'])}\n")
        lines.append("\n")
    with open(os.path.join(d, "field_dictionary_v4.md"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


# ---------------------------------------------------------------------------
# 历史代码(含 bug) 与历史测试
# ---------------------------------------------------------------------------

def render_legacy_code(v6_truth, out_dir):
    d = os.path.join(out_dir, "legacy-code")
    os.makedirs(d, exist_ok=True)
    for ver in ["v2", "v4"]:
        lines = []
        lines.append(f"// 历史实现 range_validator {ver}\n")
        lines.append(f"// 注意: 本实现来自 {ver} 时代, 含已知缺陷, "
                     "**不可**作为 v6 真值来源。\n\n")
        lines.append("#include " + '"range_validator.hpp"\n\n')
        lines.append("namespace {\n")
        lines.append("// 旧规则表(部分值已被后续版本推翻)\n")
        lines.append(f"constexpr Rule kOldRules[] = {{\n")
        for code in all_bound_codes():
            info = v6_truth[code]
            # historical_bug 陷阱: 给 buggy_value
            row = []
            for field in ["route_code", "schema_rev", "mode_flag", "phase_value"]:
                meta = info["fields"][field]
                if meta["trap"] == "historical_bug" and meta.get("buggy_version") == ver:
                    row.append((field, meta.get("buggy_value", meta["value"])))
                else:
                    row.append((field, meta["value"]))
            lines.append(f"  //{code}: " + ", ".join(f"{f}={v}" for f, v in row) + "\n")
        lines.append("};\n")
        lines.append("}  // namespace\n")
        with open(os.path.join(d, f"validator_{ver}.cpp"), "w", encoding="utf-8") as f:
            f.write("".join(lines))


def render_legacy_tests(v6_truth, out_dir):
    d = os.path.join(out_dir, "legacy-tests")
    os.makedirs(d, exist_ok=True)
    lines = []
    lines.append("// 历史黄金测试 golden_v3\n")
    lines.append("// 注意: 本测试基于 v3 时代的预期值, 部分用例已不适用于 v6。\n\n")
    lines.append('int main() {\n')
    rng = random.Random(f"{SEED}:golden")
    for code in all_bound_codes():
        info = v6_truth[code]
        # 取 v3 时代"旧值"(与 v6 不同)
        for field in ["route_code", "phase_value"]:
            meta = info["fields"][field]
            old = rng.randint(100, 999)
            lines.append(f"  // expect {code}.{field} == {old}  // v3 旧值\n")
    lines.append("  return 0;\n}\n")
    with open(os.path.join(d, "golden_v3.cpp"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


# ---------------------------------------------------------------------------
# 索引
# ---------------------------------------------------------------------------

def render_index(v6_truth, out_dir):
    d = os.path.join(out_dir, "index")
    os.makedirs(d, exist_ok=True)
    lines = []
    lines.append("# corpus 索引\n\n")
    lines.append("本目录为 range.tc 协议演进语料。结构如下：\n\n")
    lines.append("- `protocol/v1.proto` … `v6.proto`：六个版本的物模型；v6 为最新。\n")
    lines.append("- `changelog/`：v1→v2 … v5→v6 变更日志。\n")
    lines.append("- `errata/`：ERR-00 … ERR-11 勘误表（优先级最高）。\n")
    lines.append("- `adr/ADR-009.md`：文档取舍优先级。\n")
    lines.append("- `dict/field_dictionary_v4.md`：v4 字段词典（已滞后）。\n")
    lines.append("- `legacy-code/`：v2、v4 历史实现（含已知缺陷）。\n")
    lines.append("- `legacy-tests/golden_v3.cpp`：v3 历史测试（部分已失效）。\n\n")
    lines.append("## 实现绑定结构清单（48 个）\n\n")
    lines.append("| 族 | 变体 |\n|----|------|\n")
    for fam in FAMILIES:
        core = M.FAMILIES[fam]
        variants = [M.structure_code(fam, suf) for suf, _ in M.VARIANTS]
        lines.append(f"| {fam} | {', '.join(variants)} |\n")
    lines.append("\n> 提示：v6 正确值散落在上述多份文档中。务必阅读 ADR-009 的优先级规则。\n")
    with open(os.path.join(d, "structure_index.md"), "w", encoding="utf-8") as f:
        f.write("".join(lines))
