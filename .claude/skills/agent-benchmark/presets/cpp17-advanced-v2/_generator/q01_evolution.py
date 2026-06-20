# coding: utf-8
"""
Q01 长上下文题的演进模型。

设计要点:
- 复用 model.py 的 48 个实现绑定结构与 10 个适配字段维度, 但本题为"纵向时间"题:
  协议有 v1..v6 共 6 个版本, 参评 Agent 要实现【最新版 v6】的报文校验器。
- v6 的每个真值点(48 结构 x 若干维度 = 数百个点)都有一个确定的"真值来源链"与
  "陷阱类型"。来源链决定该真值出现在哪些历史文档里、以何种方式被埋藏。
- 陷阱矩阵(让模型必须读全 0.8M 语料并交叉核对, 抵抗幻觉):
    * v6_proto_full      : v6.proto 注释完整给值(约 30%, 让模型以为读 v6 就够)
    * changelog_lead     : v6 注释指向 changelog vN, 真值在那里
    * errata_overrides   : 勘误表推翻 proto 注释或 changelog
    * dict_stale         : 字段词典(v4, 最权威外表)滞后/说废弃, changelog 重新启用
    * unit_drift         : 单位在 v3 漂移(ms -> 0.1ms), 历史代码沿用旧单位
    * enum_renumber      : 枚举在 v5 重编号, 历史代码用旧编号
    * historical_bug     : 历史 cpp(v2/v4) 含已知 bug, 看似权威实则错
    * near_name          : legacy/v2 变体与 base 变体值不同, 易混用
- 文档优先级声明(埋在某份 ADR 里): 勘误表 > changelog > v6.proto 注释 > 词典 > 历史代码。
  该优先级规则本身只在 ADR 中出现一次, 是跨距离依赖的核心。

所有真值与来源链确定性可复现(固定种子)。生成器据此产出语料, 并同时输出
ground_truth.json 作为私有评分参考。
"""

import random
import model as M
from model import SEED, FAMILIES, VARIANTS, all_bound_codes, structure_code

# 6 个协议版本
VERSIONS = ["v1", "v2", "v3", "v4", "v5", "v6"]
LATEST = "v6"

# 陷阱类型
TRAPS = [
    "v6_proto_full",     # v6 注释完整
    "changelog_lead",    # v6 指向 changelog
    "errata_overrides",  # 勘误推翻
    "dict_stale",        # 词典滞后
    "unit_drift",        # 单位漂移
    "enum_renumber",     # 枚举重编号
    "historical_bug",    # 历史代码 bug
    "near_name",         # 近似命名
]

# 哪些维度适用哪些陷阱(维度 -> 允许的陷阱类型)
FIELD_TRAPS = {
    "route_code":     ["v6_proto_full", "errata_overrides", "historical_bug", "changelog_lead"],
    "schema_rev":     ["v6_proto_full", "changelog_lead", "errata_overrides"],
    "id_policy":      ["v6_proto_full", "dict_stale", "errata_overrides", "changelog_lead"],
    "replay_scope":   ["v6_proto_full", "errata_overrides", "changelog_lead", "dict_stale"],
    "stamp_required": ["v6_proto_full", "near_name", "errata_overrides"],
    "ordinal_required": ["v6_proto_full", "near_name", "errata_overrides"],
    "mode_flag":      ["enum_renumber", "historical_bug", "v6_proto_full", "near_name"],
    "phase_value":    ["enum_renumber", "historical_bug", "v6_proto_full", "changelog_lead"],
    "branch":         ["v6_proto_full", "dict_stale", "changelog_lead", "errata_overrides"],
    "band":           ["unit_drift", "historical_bug", "v6_proto_full", "errata_overrides", "changelog_lead"],
}

FIELDS_ORDER = ["route_code", "schema_rev", "id_policy", "replay_scope",
                "stamp_required", "ordinal_required", "mode_flag",
                "phase_value", "branch", "band"]


def _rng(*parts):
    return random.Random(":".join(str(p) for p in (SEED, "q01", *parts)))


def build_v6_truth():
    """生成 v6 的所有真值点, 并为每个维度分配陷阱类型与来源链。

    返回 dict: code -> { field: {value, trap, source, ...} }
    """
    codes = all_bound_codes()
    rng = _rng("v6truth")
    truth = {}
    for code in codes:
        # v6 真值(独立于 A/B)
        vals = _gen_v6_values(code, rng)
        per_field = {}
        for f in FIELDS_ORDER:
            traps_pool = FIELD_TRAPS[f]
            # 让约 25% 走"完整注释", 其余 75% 散落陷阱, 保证模型必须读全语料。
            if rng.random() < 0.25:
                trap = "v6_proto_full"
            else:
                # 从适用陷阱池中按权重选(避免 v6_proto_full 在 else 分支重复出现)
                weighted = []
                w = {"v6_proto_full": 0, "changelog_lead": 3, "errata_overrides": 3,
                     "dict_stale": 2, "unit_drift": 2, "enum_renumber": 2,
                     "historical_bug": 2, "near_name": 2}
                for tp in traps_pool:
                    weighted.extend([tp] * w.get(tp, 1))
                trap = rng.choice(weighted)
            per_field[f] = _make_field_truth(code, f, vals, trap, rng)
        truth[code] = {"fields": per_field, "vals": vals}
    return truth


def _gen_v6_values(code, rng):
    """v6 真值(确定性)。"""
    route = rng.randint(100, 999)
    rev = rng.randint(1, 9)
    idp = rng.choice(M.ID_POLICY)
    rep = rng.choice(M.REPLAY_SCOPE)
    stamp = rng.choice([True, False])
    ordinal = rng.choice([True, False])
    mf = 0
    for b in rng.sample(range(6), rng.randint(1, 5)):
        mf |= (1 << b)
    phase_idx = rng.randint(0, 5)
    branch_idx = rng.randint(0, 3)
    # band 的"语义值"为基准刻度; unit_drift 陷阱会用单位换算制造分歧
    band_min = rng.randint(0, 500)
    band_max = band_min + rng.randint(1, 1000)
    return {
        "route_code": route, "schema_rev": rev, "id_policy": idp,
        "replay_scope": rep, "stamp_required": stamp,
        "ordinal_required": ordinal, "mode_flag": mf,
        "phase_idx": phase_idx, "branch_idx": branch_idx,
        "band_min": band_min, "band_max": band_max,
        "phase_value": M.phase_value("B", phase_idx),  # v6 用 B 系编号(含 reserved)
        "branch": M.BRANCHES[branch_idx],
    }


def _make_field_truth(code, field, vals, trap, rng):
    """为单个维度构造真值 + 来源链。"""
    base = {
        "trap": trap, "field": field,
        "value": _value_of(vals, field),
    }
    # 各陷阱的额外元数据(决定语料里如何埋藏)
    if trap == "v6_proto_full":
        base.update({"source": "v6.proto 注释", "misleading": []})
    elif trap == "changelog_lead":
        # v6 注释只说"见 changelog vN", 真值在 changelog
        cv = rng.choice(["v4", "v5"])
        base.update({"source": f"changelog {cv}", "lead_to": cv})
    elif trap == "errata_overrides":
        # 勘误表某条推翻 proto 注释/changelog (errata_id 范围 0..11, 确保每条都在勘误表内)
        e = rng.randint(0, 11)
        wrong = _alt_value(vals, field, rng)
        base.update({"source": f"勘误表 ERR-{e:02d}",
                     "wrong_in_proto": wrong, "errata_id": e})
    elif trap == "dict_stale":
        # 词典(v4)说某值/废弃, changelog 重新启用真值
        wrong = _alt_value(vals, field, rng)
        base.update({"source": "changelog v5",
                     "dict_says": wrong, "dict_note": "已废弃/统一"})
    elif trap == "unit_drift":
        # band 在 v3 单位 ms->0.1ms; 真值需正确换算
        base.update({"source": "changelog v3 单位变更",
                     "old_unit": "ms", "new_unit": "0.1ms"})
    elif trap == "enum_renumber":
        # mode_flag bit 语义或 phase 编号在 v5 重编号
        base.update({"source": "changelog v5 枚举重编号",
                     "old_value": _alt_value(vals, field, rng)})
    elif trap == "historical_bug":
        # 历史 cpp(v2/v4) 含 bug, 给了错值; v6.proto 注释给的是正确值,
        # 但历史代码与历史测试会诱导沿用 buggy 值。真值来源仍为 v6.proto。
        hv = rng.choice(["v2", "v4"])
        base.update({"source": "v6.proto 注释(历史代码 v2/v4 含已知 bug, 不可信)",
                     "buggy_version": hv,
                     "buggy_value": _alt_value(vals, field, rng)})
    elif trap == "near_name":
        # legacy/v2 变体值不同
        base.update({"source": "v6.proto 注释(注意变体差异)",
                     "neighbor": _alt_value(vals, field, rng)})
    return base


def _value_of(vals, field):
    if field == "band":
        return [vals["band_min"], vals["band_max"]]
    if field in vals:
        return vals[field]
    return None


def _alt_value(vals, field, rng):
    """生成一个不同于真值的"误导值"。"""
    v = _value_of(vals, field)
    if field in ("stamp_required", "ordinal_required"):
        return not v
    if field == "id_policy":
        return rng.choice([x for x in M.ID_POLICY if x != v] or M.ID_POLICY)
    if field == "replay_scope":
        return rng.choice([x for x in M.REPLAY_SCOPE if x != v] or M.REPLAY_SCOPE)
    if field == "branch_idx" or field == "branch":
        idx = vals["branch_idx"]
        return M.BRANCHES[rng.choice([x for x in range(4) if x != idx] or [0])]
    if field == "phase_value":
        return rng.choice([x for x in [0, 1, 2, 3, 5, 6] if x != v] or [0])
    if field == "route_code":
        alternate = rng.randint(100, 999)
        return alternate if alternate != v else (100 if v != 100 else 101)
    if field == "schema_rev":
        alternate = rng.randint(1, 9)
        return alternate if alternate != v else (1 if v != 1 else 2)
    if field == "mode_flag":
        mf = 0
        for b in rng.sample(range(6), rng.randint(1, 5)):
            mf |= (1 << b)
        return mf if mf != v else mf + 1
    if field == "band":
        alternate = [rng.randint(0, 500), rng.randint(0, 500)]
        return alternate if alternate != v else [v[0], v[1] + 1]
    return v


# 文档优先级(埋在 ADR 里, 只出现一次)
PRIORITY_RULE = ("勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码")

if __name__ == "__main__":
    t = build_v6_truth()
    # 统计陷阱分布
    from collections import Counter
    c = Counter()
    for code, info in t.items():
        for f, meta in info["fields"].items():
            c[meta["trap"]] += 1
    total = sum(c.values())
    print(f"真值点总数: {total}")
    for k, v in c.most_common():
        print(f"  {k}: {v} ({v/total:.0%})")
