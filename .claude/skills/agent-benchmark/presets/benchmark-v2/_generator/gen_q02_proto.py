# coding: utf-8
"""
Q02 协议适配: 生成系统 A(range.tc)与系统 B(orbit.tc)两套物模型,以及变更映射。

输出到 q02-protocol-adaptation/系统A 与 系统B 目录。

设计:
- 系统A: range.tc, 已完成一次结构契约升级(协议一)。每个实现绑定结构在 A 上有
  一组"已完成"的真值(由 perturb_for_a 生成,与 B 不同)。
- 系统B: orbit.tc, 适配目标(协议二)。每个实现绑定结构在 B 上有最终真值。
- 除 48 个实现绑定结构外, 再生成约 600 个非绑定上下文结构(噪声), 模拟真实物模型
  的子系统庞大与低信噪比。非绑定结构名称与绑定结构近似(制造近似命名陷阱)。
- 每个结构的协议注释里逐字段写出该结构的适配契约字段值。系统 A 的注释写出 A 值;
  系统B 的注释写出 B 值。题目要求把规则表从"A 的起始值"改到"B 的最终值"。
"""

import os
import random

import model
from model import (SEED, FAMILIES, VARIANTS, all_bound_codes, structure_code,
                   gen_field_values, perturb_for_a,
                   ID_POLICY_CN, REPLAY_SCOPE_CN, MODE_BITS_CN, BRANCHES_CN,
                   phase_value, phase_name, FAMILY_PROTO_FIELDS)


# 系统名与包名(去 tsl 血统)
PKG_A = "range.tc"      # 测控靶区.遥测指令
PKG_B = "orbit.tc"      # 轨道.遥测指令


def _field_comment(system, vals):
    """生成一个结构注释块里的适配契约字段说明。"""
    bits_desc = " ".join(
        f"{name}={int(bool(vals['mode_flag'] & (1 << i)))}"
        for i, name in enumerate(model.MODE_BITS)
    )
    return (
        f"  // 适配契约元数据(本系统 {system} 的真值, 逐字段独立, 不从名称推断):\n"
        f"  //   路由码 route_code = {vals['route_code']}\n"
        f"  //   小版本 schema_rev = {vals['schema_rev']}\n"
        f"  //   标识策略 id_policy = {ID_POLICY_CN[vals['id_policy']]}\n"
        f"  //   回放域 replay_scope = {REPLAY_SCOPE_CN[vals['replay_scope']]}\n"
        f"  //   需要时间戳 stamp_required = {str(vals['stamp_required']).lower()}\n"
        f"  //   需要序号 ordinal_required = {str(vals['ordinal_required']).lower()}\n"
        f"  //   模式位 mode_flag = {vals['mode_flag']}  ({bits_desc})\n"
        f"  //   飞行阶段 phase = {phase_name(system, vals['phase_idx'])}={vals['phase_value']}\n"
        f"  //   载荷分支 branch = {BRANCHES_CN[vals['branch']]}\n"
        f"  //   合法区间 band = [{vals['band_min']}, {vals['band_max']}]\n"
    )


def _render_structure(system, code, vals, family, bound, seq):
    """渲染单个 message。bound=True 表示实现绑定结构(参与迁移)。"""
    fields = FAMILY_PROTO_FIELDS[family]
    body = []
    # 真实物模型字段(上下文噪声)
    for i, (ftype, fname, fcomment) in enumerate(fields, start=1):
        body.append(f"  {ftype} {fname} = {i};  // {fcomment}\n")
    # 适配契约字段(只在注释里给出真值, 不在 proto 真正定义, 由 C++ 规则表承载)
    comment_lines = []
    if bound:
        comment_lines.append(_field_comment(system, vals))
    else:
        comment_lines.append(
            "  // 本结构仅为上下游业务上下文, 不参与实现绑定迁移。\n"
            "  // 不得据名称、相邻位置或所属子系统为本结构新增 C++ 规则项。\n"
        )
    head = (
        f"// 结构编号 {system}-{seq:03d}。\n"
        f"// 是否实现绑定: {('是' if bound else '否')}。\n"
        f"// 结构代号: {code}。\n"
        f"// 所属子系统族: {family}。\n"
    )
    msg = f"{head}{''.join(comment_lines)}message {code} {{\n{''.join(body)}\n}}\n\n"
    return msg


def _noise_codes(bound_set, n, rng):
    """生成 n 个非绑定上下文结构代号, 名称与绑定结构近似(陷阱)。"""
    noises = []
    base_words = []
    for fam in FAMILIES:
        base_words.append(model.FAMILIES[fam])
    # 近似后缀池: 与 VARIANTS 仅一两个字符不同
    near_suffixes = ["", "s", "_view", "_patch", "_legacy", "_v2",
                     "_views", "_patches", "_legacy2", "_v3", "_summary",
                     "_record", "_records", "_snapshot", "_delta",
                     "_ctx", "_context", "_0", "_1", "_log", "_history"]
    attempts = 0
    while len(noises) < n and attempts < n * 20:
        attempts += 1
        fam = rng.choice(list(FAMILIES.keys()))
        core = model.FAMILIES[fam]
        suf = rng.choice(near_suffixes)
        code = core + suf
        if code in bound_set or code in noises:
            continue
        noises.append((fam, code))
    return noises


def build_system_proto(system, pkg, out_dir, bound_map, total_noise, bound_fam):
    """渲染单个系统的完整 .proto 物模型文件。"""
    rng = random.Random(f"{SEED}:noise:{system}")
    bound_set = set(bound_map.keys())
    noise = _noise_codes(bound_set, total_noise, rng)

    # 混排: 绑定与非绑定交错, 模拟真实物模型结构散落
    items = []  # (family, code, bound, vals)
    for code in bound_map:  # 保持绑定结构稳定顺序
        fam = bound_fam[code]
        items.append((fam, code, True, bound_map[code]))
    for fam, code in noise:
        items.append((fam, code, False, None))
    rng.shuffle(items)

    # 分文件: definitions / status / action / config / event 四象限 + definitions
    os.makedirs(out_dir, exist_ok=True)

    # definitions.proto: enum 与公共类型
    defs = []
    defs.append('syntax = "proto3";\n\n')
    defs.append(f"package {pkg};\n\n")
    defs.append("// 公共定义: 帧时间戳、应答、枚举与按位 flag。\n\n")
    defs.append("message Frame { int64 stamp = 1; }\n")  # 仿 tsl Attribute
    defs.append("message Ack { bool ok = 1; int32 code = 2; }\n\n")
    defs.append("// 标识策略(不透明枚举)\n")
    defs.append("enum IdPolicy { STRICT = 0; KEEP_BLANK = 1; AUTO_GEN = 2; }\n\n")
    defs.append("// 回放域\n")
    defs.append("enum ReplayScope { NONE = 0; STAGE = 1; VEHICLE = 2; RANGE = 3; }\n\n")
    defs.append("// 飞行阶段(注意: 系统 A 与系统 B 的编号不同)\n")
    defs.append("enum Phase {\n")
    if system == "A":
        for name, val in model.PHASE_A:
            defs.append(f"  {name.upper()} = {val};\n")
    else:
        # B: 列出含 reserved 占位的完整表
        full = [("idle", 0), ("standby", 1), ("boost", 2),
                ("ascent", 3), ("reserved", 4), ("coast", 5), ("insertion", 6)]
        for name, val in full:
            defs.append(f"  {name.upper()} = {val};\n")
    defs.append("}\n\n")
    defs.append("// 模式位(按位 bit0..bit5, 逐位独立语义)\n")
    defs.append("enum ModeBit {\n")
    for i, name in enumerate(model.MODE_BITS):
        cn = MODE_BITS_CN[name]
        defs.append(f"  MB_{name.upper()} = {1 << i};  // bit{i}: {cn}\n")
    defs.append("}\n\n")

    with open(os.path.join(out_dir, "definitions.proto"), "w", encoding="utf-8") as f:
        f.write("".join(defs))

    # 四象限文件: 按结构所属子系统族分配(模拟物模型分区)
    quadrant = {"status.proto": [], "action.proto": [],
                "config.proto": [], "event.proto": []}
    # 简单分配: 按 family 哈希到四象限
    fam_order = list(FAMILIES.keys())
    for idx, (fam, code, bound, vals) in enumerate(items):
        qi = fam_order.index(fam) % 4
        keys = list(quadrant.keys())
        quadrant[keys[qi]].append((fam, code, bound, vals, idx + 1))

    for fname, entries in quadrant.items():
        head = (
            f'syntax = "proto3";\n\n'
            f"package {pkg};\n\n"
            f'import "definitions.proto";\n\n'
            f"// {fname} 分区: 含本系统物模型结构。\n"
            f"// 注意: 仅标注'是否实现绑定: 是'的结构参与迁移适配; 其余为上下文。\n"
            f"// 适配契约字段值逐结构独立, 结构代号相近不代表字段相同。\n\n"
        )
        body = []
        for fam, code, bound, vals, seq in entries:
            body.append(_render_structure(system, code, vals, fam, bound, seq))
        with open(os.path.join(out_dir, fname), "w", encoding="utf-8") as f:
            f.write(head + "".join(body))

    return len(items)


def build_change_map(bound_map_a, bound_map_b, out_path):
    """生成协议一(系统A)到协议二(系统B)的变更映射.json。"""
    import json
    items = []
    seq = 1
    a_struct_map = {}  # code -> (a_seq, b_seq) 占位
    # 先按绑定结构顺序生成映射
    for code in bound_map_b:
        a_vals = bound_map_a[code]
        b_vals = bound_map_b[code]
        before = (
            f"路由码{a_vals['route_code']}、小版本{a_vals['schema_rev']}、"
            f"标识{ID_POLICY_CN[a_vals['id_policy']]}、"
            f"回放{REPLAY_SCOPE_CN[a_vals['replay_scope']]}、"
            f"时间戳{str(a_vals['stamp_required']).lower()}、"
            f"序号{str(a_vals['ordinal_required']).lower()}、"
            f"模式位0x{a_vals['mode_flag']:x}、"
            f"阶段{phase_name('A', a_vals['phase_idx'])}、"
            f"分支{BRANCHES_CN[a_vals['branch']]}"
        )
        items.append({
            "变更编号": f"适配-{seq:03d}",
            "C++结构代号": code,
            "协议一结构": f"range.tc:{code}",
            "协议二结构": f"orbit.tc:{code}",
            "协议一变更后(系统A值)": before,
            "迁移提醒": "协议一值仅描述迁移动机; 协议二卡片才是最终真值, 不得直接复制。",
        })
        seq += 1
    obj = {
        "变更集名称": "range.tc 向 orbit.tc 的测控结构适配",
        "说明": "系统 A(range.tc)已完成一次结构契约升级; 现需适配到相似但不同的系统 B(orbit.tc)。仅适配实现绑定为是的 48 个结构。",
        "变更项": items,
        "统一限制": [
            "只适配实现绑定为是的 48 个结构",
            "不得修改公开 C++ 头文件、测试或协议工件",
            "不得为上下游相邻或近似命名结构新增规则项",
            "系统 A 与系统 B 的飞行阶段编号不同, 不得照搬",
        ],
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)


def main(preset_dir):
    codes = all_bound_codes()
    # 先生成 B 真值(目标), 再由 B 生成 A 的独立真值
    bound_b = {c: gen_field_values("B", c) for c in codes}
    bound_a = {c: perturb_for_a(bound_b[c], c) for c in codes}

    base = os.path.join(preset_dir, "q02-protocol-adaptation")
    dir_a = os.path.join(base, "系统A")
    dir_b = os.path.join(base, "系统B")
    # 绑定结构代号 -> 所属族 映射(确定性)
    bound_fam = {}
    for fam in FAMILIES:
        for suf, _vname in VARIANTS:
            bound_fam[structure_code(fam, suf)] = fam
    n_a = build_system_proto("A", PKG_A, dir_a, bound_a, total_noise=300, bound_fam=bound_fam)
    n_b = build_system_proto("B", PKG_B, dir_b, bound_b, total_noise=300, bound_fam=bound_fam)
    build_change_map(bound_a, bound_b, os.path.join(dir_a, "变更映射.json"))

    # 真值快照(私有, 仅评分参考用)
    import json
    truth = {
        "system_A": bound_a,
        "system_B": bound_b,
        "bound_count": len(codes),
        "noise_count_A": n_a - len(codes),
        "noise_count_B": n_b - len(codes),
    }
    private = os.path.join(base, "_private")
    os.makedirs(private, exist_ok=True)
    with open(os.path.join(private, "q02_truth.json"), "w", encoding="utf-8") as f:
        json.dump(truth, f, ensure_ascii=False, indent=2)
    print(f"Q02 物模型已生成: A 结构 {n_a} (绑定 {len(codes)}), "
          f"B 结构 {n_b} (绑定 {len(codes)})")
    return truth


if __name__ == "__main__":
    import sys
    preset_dir = sys.argv[1] if len(sys.argv) > 1 else os.pardir
    main(preset_dir)
