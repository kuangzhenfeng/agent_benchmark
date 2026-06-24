# coding: utf-8
"""
Q01 长上下文语料生成器。

把 q01_evolution.build_v6_truth() 的真值与陷阱链渲染成约 0.8M token 的自洽语料,
输出到 q01-long-context-protocol/corpus/。

语料构成:
  corpus/
    README.md            语料导航与版本时间线(最先被读, 故意不含全部真值)
    protocol/
      v1.proto .. v6.proto    六个版本的物模型(v6.proto 注释不全, 故意)
    changelog/
      v1-to-v2.md .. v5-to-v6.md   五份变更日志(含真值)
    errata/
      ERR-00.md .. ERR-11.md       十二份勘误(推翻 proto/changelog)
    adr/
      ADR-009.md            含【文档优先级】规则(只此一处)
    dict/
      field_dictionary_v4.md      字段词典(v4, 滞后/部分废弃)
    legacy-code/
      validator_v2.cpp validator_v4.cpp   历史实现(含已知 bug)
    legacy-tests/
      golden_v3.cpp               历史测试(v3 时代, 部分已不适用 v6)
    index/
      structure_index.md          结构代号索引(导航用)

陷阱如何落地(见 q01_evolution.TRAPS):
- 每个真值点根据其 trap 类型, 决定该值/误导值出现在哪个文档、以何种措辞。
- 模型必须按 ADR-009 的优先级规则取舍, 才能得到 v6 正确值。
"""

import os
import json
import model as M
from model import SEED, FAMILIES, VARIANTS, all_bound_codes, structure_code
import q01_evolution as E
from q01_evolution import VERSIONS, LATEST, PRIORITY_RULE


# ---------------------------------------------------------------------------
# 渲染辅助
# ---------------------------------------------------------------------------

def _cn_id_policy(v):
    return {"strict": "严格必填", "keep_blank": "保留空值", "auto_gen": "空则生成"}[v]


def _cn_replay(v):
    return {"none": "不回放", "stage": "本级", "vehicle": "本箭", "range": "本靶区"}[v]


def _cn_branch(v):
    return {"telemetry": "遥测", "command": "指令", "config": "配置", "event": "事件"}[v]


def _bits_desc(mf):
    return " ".join(f"{n}={int(bool(mf & (1 << i)))}"
                    for i, n in enumerate(M.MODE_BITS))


def _phase_label(idx):
    """v6 用 B 系编号含 reserved。"""
    names = ["idle", "standby", "boost", "ascent", "reserved", "coast", "insertion"]
    nums = [0, 1, 2, 3, 4, 5, 6]
    # idx 是语义序 0..5 -> B 编号
    bnum = M.PHASE_B_NUMBER[idx]
    bname = M.PHASE_B_SEMANTIC[idx]
    return bname, bnum


# ---------------------------------------------------------------------------
# proto 渲染(六版)
# ---------------------------------------------------------------------------

def _noise_codes(bound_set, n, rng, family_keys):
    """生成 n 个非绑定上下文结构代号, 名称与绑定结构近似(陷阱)。"""
    noises = []
    near_suffixes = ["", "s", "_view", "_patch", "_legacy", "_v2",
                     "_views", "_patches", "_legacy2", "_v3", "_summary",
                     "_record", "_records", "_snapshot", "_delta",
                     "_ctx", "_context", "_0", "_1", "_2", "_log", "_history",
                     "_raw", "_enriched", "_mirror", "_shadow", "_tmp", "_bak"]
    attempts = 0
    while len(noises) < n and attempts < n * 30:
        attempts += 1
        fam = rng.choice(family_keys)
        core = M.FAMILIES[fam]
        suf = rng.choice(near_suffixes)
        code = core + suf
        if code in bound_set or code in noises:
            continue
        noises.append((fam, code))
    return noises


def render_proto(version, v6_truth, out_dir, total_structures):
    """渲染某版本的 .proto。v6 故意注释不全(陷阱); v1-v5 为历史演进。

    total_structures 控制每个版本的 message 总数(含噪声), 用于撑大体量。
    """
    is_latest = (version == LATEST)
    lines = []
    lines.append('syntax = "proto3";\n\n')
    lines.append(f"package range.tc.{version};\n\n")
    lines.append(f"// {version} 物模型。")
    if is_latest:
        lines.append("// 本文件为最新版本; 但部分字段注释指向 changelog/勘误表, "
                     "需结合 corpus 内其他文档交叉核对。\n")
    else:
        lines.append(f"// 历史版本, 已被后续版本取代, 仅作演进参考。\n")
    lines.append("\n")

    # definitions
    lines.append("message Frame { int64 stamp = 1; }\n")
    lines.append("enum IdPolicy { STRICT=0; KEEP_BLANK=1; AUTO_GEN=2; }\n")
    lines.append("enum ReplayScope { NONE=0; STAGE=1; VEHICLE=2; RANGE=3; }\n")
    # 阶段 enum: v1-v4 用旧编号, v5+ 用新编号(B系含 reserved)
    if version in ("v1", "v2", "v3", "v4"):
        lines.append("// 飞行阶段(旧编号)\nenum Phase { IDLE=0; READY=1; LIFTOFF=2; "
                     "ASCENT=3; COAST=4; INSERTION=5; }\n")
    else:
        lines.append("// 飞行阶段(v5 起重编号, 新增 RESERVED 占位)\nenum Phase { IDLE=0; "
                     "STANDBY=1; BOOST=2; ASCENT=3; RESERVED=4; COAST=5; INSERTION=6; }\n")
    lines.append("enum ModeBit { MB_ARM=1; MB_IGNITE=2; MB_HOLD=4; MB_SAFE=8; "
                 "MB_REDUNDANT=16; MB_DEBUG=32; }\n\n")

    import random
    rng = random.Random(f"{SEED}:proto-noise:{version}")
    codes = all_bound_codes()
    bound_set = set(codes)
    family_keys = list(FAMILIES.keys())
    # 绑定结构代号 -> 族
    bound_fam = {}
    for f in FAMILIES:
        for suf, _v in M.VARIANTS:
            bound_fam[M.structure_code(f, suf)] = f
    # 噪声结构
    n_noise = max(0, total_structures - len(codes))
    noises = _noise_codes(bound_set, n_noise, rng, family_keys)

    # 混排
    items = []  # (fam, code, is_bound)
    for code in codes:
        items.append((bound_fam[code], code, True))
    for fam, code in noises:
        items.append((fam, code, False))
    rng.shuffle(items)

    for fam, code, is_bound in items:
        fields = M.FAMILY_PROTO_FIELDS[fam]
        lines.append(f"message {code} {{\n")
        for i, (ftype, fname, fcomment) in enumerate(fields, start=1):
            lines.append(f"  {ftype} {fname} = {i};  // {fcomment}\n")

        if is_latest and is_bound:
            # v6: 按陷阱类型决定注释完整度
            info = v6_truth[code]
            lines.append(_render_v6_field_comments(code, info, lines, out_dir))
        elif is_latest:
            # v6 非绑定: 上下文说明
            lines.append("  // 本结构为上下游业务上下文, 不参与实现绑定迁移。\n")
        else:
            # 历史版本: 给历史值(与 v6 不同), 制造"看起来权威实则过时"
            lines.append(_render_legacy_field_comments(version, code))
        lines.append("}\n\n")

    os.makedirs(os.path.join(out_dir, "protocol"), exist_ok=True)
    with open(os.path.join(out_dir, "protocol", f"{version}.proto"), "w",
              encoding="utf-8") as f:
        f.write("".join(lines))


def _value_text(field, val):
    """字段值的中文文本(供 changelog/errata/dict 复用)。"""
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


def _render_v6_field_comments(code, info, lines_acc, out_dir):
    """v6.proto 的字段注释: 按陷阱决定给真值/指向/误导。"""
    out = []
    out.append("  // 适配契约(v6):\n")
    for field in E.FIELDS_ORDER:
        meta = info["fields"][field]
        trap = meta["trap"]
        val = meta["value"]
        if trap == "v6_proto_full":
            out.append(_comment_line(field, val))
        elif trap == "changelog_lead":
            lead = meta.get("lead_to", "v5")
            next_version = VERSIONS[VERSIONS.index(lead) + 1]
            out.append(f"  //   {field}: 本版本起变更, 详见 changelog/{lead}-to-{next_version}.md\n")
        elif trap == "errata_overrides":
            wrong = meta.get("wrong_in_proto")
            # proto 里给的是"错误值", 勘误表纠正
            out.append(_comment_line(field, wrong))
        elif trap == "dict_stale":
            out.append(_comment_line(field, val))
        elif trap == "unit_drift":
            out.append(f"  //   {field}: 单位自 v3 起变更, 见 changelog/v2-to-v3.md\n")
        elif trap == "enum_renumber":
            old = meta.get("old_value")
            out.append(f"  //   {field}: v5 枚举重编号(旧值 {old}), 以重编号后为准\n")
            out.append(_comment_line(field, val))
        elif trap == "historical_bug":
            # v6.proto 注释给出正确值; 历史 cpp/测试的 buggy 值是诱饵
            out.append(_comment_line(field, val))
        elif trap == "near_name":
            out.append(_comment_line(field, val))
    return "".join(out)


def _comment_line(field, val):
    if field == "band":
        return f"  //   {field}: [{val[0]}, {val[1]}]\n"
    if field in ("stamp_required", "ordinal_required"):
        return f"  //   {field}: {str(val).lower()}\n"
    if field == "id_policy":
        return f"  //   {field}: {_cn_id_policy(val)}\n"
    if field == "replay_scope":
        return f"  //   {field}: {_cn_replay(val)}\n"
    if field == "branch":
        return f"  //   {field}: {_cn_branch(val)}\n"
    if field == "mode_flag":
        return f"  //   {field}: {val} ({_bits_desc(val)})\n"
    if field == "phase_value":
        return f"  //   {field}: {val}\n"
    return f"  //   {field}: {val}\n"


def _render_legacy_field_comments(version, code):
    """历史版本的字段注释: 给与 v6 不同的历史值。"""
    import random
    rng = random.Random(f"{SEED}:legacy:{version}:{code}")
    out = ["  // 适配契约(历史值, 已过时):\n"]
    # 历史值随机扰动
    route = rng.randint(100, 999)
    rev = rng.randint(1, 9)
    idp = rng.choice(M.ID_POLICY)
    rep = rng.choice(M.REPLAY_SCOPE)
    stamp = rng.choice([True, False])
    ordinal = rng.choice([True, False])
    mf = 0
    for b in rng.sample(range(6), rng.randint(1, 5)):
        mf |= (1 << b)
    phase = rng.choice([0, 1, 2, 3, 4, 5])
    branch = rng.choice(M.BRANCHES)
    out.append(f"  //   route_code: {route}\n  //   schema_rev: {rev}\n")
    out.append(f"  //   id_policy: {_cn_id_policy(idp)}\n")
    out.append(f"  //   replay_scope: {_cn_replay(rep)}\n")
    out.append(f"  //   stamp_required: {str(stamp).lower()}\n")
    out.append(f"  //   ordinal_required: {str(ordinal).lower()}\n")
    out.append(f"  //   mode_flag: {mf}\n  //   phase_value: {phase}\n")
    out.append(f"  //   branch: {_cn_branch(branch)}\n")
    return "".join(out)
