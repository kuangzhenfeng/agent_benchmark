# coding: utf-8
"""
Q01 编排: 生成全部语料 + C++ 公开头/starter/公开检查 + ground_truth。

用法: python3 gen_q01_main.py <preset_dir>
"""

import os
import sys
import json

import model as M
from model import SEED, FAMILIES, all_bound_codes
import q01_evolution as E
from gen_q01_corpus import render_proto
from gen_q01_docs import (render_changelogs, render_errata, render_adr,
                          render_dictionary, render_legacy_code,
                          render_legacy_tests, render_index)


def build_corpus(preset_dir):
    v6_truth = E.build_v6_truth()
    base = os.path.join(preset_dir, "q01-long-context-protocol")
    corpus = os.path.join(base, "corpus")
    # 清空 corpus(避免旧残留)
    import shutil
    if os.path.isdir(corpus):
        shutil.rmtree(corpus)

    # 每个版本 message 总数(含噪声), 撑大体量。中文语料 UTF-8 约 3 字节/字,
    # token 约 1.5~2 字符/token; 目标 ~0.8M token -> ~1.4M~1.6M 字符。
    structures_per_version = 560
    # 六版 proto
    for ver in E.VERSIONS:
        render_proto(ver, v6_truth, corpus, structures_per_version)
    # 其余文档
    render_changelogs(v6_truth, corpus)
    render_errata(v6_truth, corpus)
    render_adr(corpus)
    render_dictionary(v6_truth, corpus)
    render_legacy_code(v6_truth, corpus)
    render_legacy_tests(v6_truth, corpus)
    render_index(v6_truth, corpus)
    # README 导航
    _render_corpus_readme(corpus, v6_truth)
    return v6_truth


def _render_corpus_readme(corpus, v6_truth):
    lines = []
    lines.append("# range.tc 协议演进语料\n\n")
    lines.append("本目录是 range.tc 测控协议从 v1 到 v6 的完整演进资料。"
                 "任务是实现 v6 的报文校验器，正确值需要结合本目录下多份文档交叉核对。\n\n")
    lines.append("## 阅读顺序建议\n\n")
    lines.append("1. `index/structure_index.md`：结构清单与目录导航。\n")
    lines.append("2. `adr/ADR-009.md`：文档取舍优先级（重要）。\n")
    lines.append("3. `protocol/v6.proto`：最新物模型（注释不全，需配合 changelog/勘误）。\n")
    lines.append("4. `changelog/`、`errata/`、`dict/`：补充与订正。\n")
    lines.append("5. `legacy-code/`、`legacy-tests/`：历史实现与测试（已知过时，谨慎参考）。\n\n")
    lines.append("> 提示：不要只读 v6.proto。许多字段的真值在 changelog 或勘误表中，"
                 "且部分 proto 注释与历史代码的值已被推翻。\n")
    with open(os.path.join(corpus, "README.md"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


def gen_cpp(v6_truth, preset_dir):
    """生成 Q01 的 C++ 公开头/starter/公开检查。"""
    base = os.path.join(preset_dir, "q01-long-context-protocol")
    # 复用 Q02 的头文件结构(同样的 Structure/FieldRule/派生函数)
    q02_hpp = os.path.join(preset_dir, "q02-protocol-adaptation", "include",
                           "protocol_adapter.hpp")
    hpp = q02_hpp.replace("协议适配", "长上下文").replace("range.tc", "range.tc")
    # 直接复用同一份头(结构相同), 但题目语义不同, 故另写一份
    inc = os.path.join(base, "include")
    os.makedirs(inc, exist_ok=True)
    # 头文件与 Q02 一致(同样 48 结构 10 维度), 但说明改为"v6 校验器"
    with open(q02_hpp, encoding="utf-8") as f:
        hpp_text = f.read()
    hpp_text = hpp_text.replace(
        "// 协议适配公开 API(不可修改)。",
        "// range.tc v6 校验器公开 API(不可修改)。")
    hpp_text = hpp_text.replace(
        "// 实现请写在 src/protocol_adapter.cpp。",
        "// 实现请写在 src/range_validator.cpp。")
    hpp_text = hpp_text.replace(
        "// 业务背景见 QUESTION.md 与 系统A/系统B/变更映射.json。\n"
        "// 规则表的正确值以「系统B」物模型注释中的真值为准, 逐字段独立读取。",
        "// 业务背景见 QUESTION.md 与 corpus/。规则表的正确值为 range.tc v6 真值, "
        "散落在 corpus 多份文档中, 须按 ADR-009 优先级取舍。")
    hpp_text = hpp_text.replace('src/protocol_adapter.cpp', 'src/range_validator.cpp')
    with open(os.path.join(inc, "range_validator.hpp"), "w", encoding="utf-8") as f:
        f.write(hpp_text)

    # starter: 给"历史代码风格"的错误起始(沿用 v4 旧值), 让模型必须改正
    _gen_starter(v6_truth, base)
    # 公开检查: 抽样 8 个结构 + 派生语义, 用 v6 真值
    _gen_public_checks(v6_truth, base)


def _gen_starter(v6_truth, base):
    """starter 故意填历史/误导值, 体现"未读全语料"的错误直觉。"""
    import random
    rng = random.Random(f"{SEED}:q01:starter")
    codes = all_bound_codes()
    rows = []
    for c in codes:
        info = v6_truth[c]
        # starter 取每个字段的"proto 或历史错误值"(模拟只读 v6.proto + 抄历史代码)
        row = []
        for field in E.FIELDS_ORDER:
            meta = info["fields"][field]
            # 错误直觉值
            if meta["trap"] == "errata_overrides":
                v = meta.get("wrong_in_proto", meta["value"])
            elif meta["trap"] == "historical_bug":
                v = meta.get("buggy_value", meta["value"])
            elif meta["trap"] == "dict_stale":
                v = meta.get("dict_says", meta["value"])
            elif meta["trap"] == "enum_renumber":
                v = meta.get("old_value", meta["value"])
            elif meta["trap"] == "unit_drift":
                # 旧单位值
                v = meta["value"]
            else:
                v = meta["value"]
            row.append((field, v))
        rows.append((c, row))
    lines = []
    lines.append('#include "range_validator.hpp"\n\n#include <array>\n\nnamespace {\n\n')
    lines.append("// 规则表: 当前为【未读全语料的错误直觉值】。\n")
    lines.append("// 参评 Agent 须结合 corpus 多份文档, 按 ADR-009 优先级校正为 v6 真值。\n")
    lines.append(f"constexpr std::array<FieldRule, {len(codes)}> kRules{{{{\n")
    for c, row in rows:
        lines.append(_rule_cpp_row(c, row))
    lines.append("}};\n\n}  // 匿名命名空间\n\n")
    lines.append(_derive_impl())
    src = os.path.join(base, "src")
    os.makedirs(src, exist_ok=True)
    with open(os.path.join(src, "range_validator.cpp"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


def _rule_cpp_row(code, row):
    d = dict(row)
    pol = {"strict": "IdPolicy::strict", "keep_blank": "IdPolicy::keep_blank",
           "auto_gen": "IdPolicy::auto_gen"}[d["id_policy"]]
    rep = {"none": "ReplayScope::none", "stage": "ReplayScope::stage",
           "vehicle": "ReplayScope::vehicle", "range": "ReplayScope::range"}[d["replay_scope"]]
    br = {"telemetry": "Branch::telemetry", "command": "Branch::command",
          "config": "Branch::config", "event": "Branch::event"}[d["branch"]]
    bmin, bmax = d["band"][0], d["band"][1]
    return (f"    {{Structure::{code}, {d['route_code']}, {d['schema_rev']}, "
            f"{pol}, {rep}, {'true' if d['stamp_required'] else 'false'}, "
            f"{'true' if d['ordinal_required'] else 'false'}, "
            f"{d['mode_flag']:#04x}u, {d['phase_value']}, {br}, {bmin}, {bmax}}},\n")


def _derive_impl():
    return r'''std::optional<FieldRule> rule_for(Structure id) {
    for (const FieldRule& r : kRules) {
        if (r.id == id) return r;
    }
    return std::nullopt;
}

bool accepts(Structure id, const Incoming& in) {
    const auto r = rule_for(id);
    if (!r) return false;
    if (r->id_policy == IdPolicy::strict && in.identifier.empty()) return false;
    if (r->stamp_required && !in.has_stamp) return false;
    if (r->ordinal_required && !in.has_ordinal) return false;
    return true;
}

bool replayable(Structure id) {
    const auto r = rule_for(id);
    return r && r->replay_scope != ReplayScope::none;
}

bool flag_active(Structure id, ModeBit bit) {
    const auto r = rule_for(id);
    if (!r) return false;
    return (r->mode_flag & (std::uint32_t{1} << static_cast<unsigned>(bit))) != 0;
}

int phase_rank(Structure id) {
    const auto r = rule_for(id);
    return r ? static_cast<int>(r->phase_value) : -1;
}

Branch branch_of(Structure id) {
    const auto r = rule_for(id);
    return r ? r->branch : Branch::telemetry;
}

bool within_band(Structure id, std::int32_t sample) {
    const auto r = rule_for(id);
    if (!r) return false;
    return sample >= r->band_min && sample <= r->band_max;
}
'''


def _gen_public_checks(v6_truth, base):
    codes = all_bound_codes()
    sample = [codes[0], codes[7], codes[11], codes[16], codes[23],
              codes[30], codes[37], codes[45]]
    lines = []
    lines.append('#include "range_validator.hpp"\n\n')
    lines.append('#include <cassert>\n#include <cstdint>\n\nnamespace {\n\n')
    lines.append('void check_one(Structure id, const FieldRule& exp) {\n')
    lines.append('    const auto r = rule_for(id);\n')
    lines.append('    assert(r.has_value());\n')
    for f in ["route_code", "schema_rev", "id_policy", "replay_scope",
              "stamp_required", "ordinal_required", "mode_flag",
              "phase_value", "branch", "band_min", "band_max"]:
        lines.append(f'    assert(r->{f} == exp.{f});\n')
    lines.append('    assert(replayable(id) == (exp.replay_scope != ReplayScope::none));\n')
    lines.append('    assert(phase_rank(id) == exp.phase_value);\n')
    lines.append('    assert(branch_of(id) == exp.branch);\n')
    lines.append('    assert(within_band(id, exp.band_min));\n')
    lines.append('    assert(within_band(id, exp.band_max));\n')
    lines.append('    assert(!within_band(id, exp.band_min - 1));\n')
    lines.append('    assert(!within_band(id, exp.band_max + 1));\n')
    lines.append('}\n\n}  // 匿名命名空间\n\nint main() {\n')
    for c in sample:
        info = v6_truth[c]["vals"]
        vals = {
            "route_code": info["route_code"], "schema_rev": info["schema_rev"],
            "id_policy": info["id_policy"], "replay_scope": info["replay_scope"],
            "stamp_required": info["stamp_required"],
            "ordinal_required": info["ordinal_required"],
            "mode_flag": info["mode_flag"], "phase_value": info["phase_value"],
            "branch": info["branch"],
            "band": [info["band_min"], info["band_max"]],
        }
        lines.append("    check_one(Structure::" + c + ", " +
                     _rule_row(c, vals) + ");\n")
    c0 = sample[0]
    v0 = v6_truth[c0]["vals"]
    lines.append(f"    // flag_active 抽样: {c0}\n")
    for i, name in enumerate(M.MODE_BITS):
        active = bool(v0["mode_flag"] & (1 << i))
        lines.append(f"    assert(flag_active(Structure::{c0}, ModeBit::{name}) "
                     f"== {'true' if active else 'false'});\n")
    lines.append('    return 0;\n}\n')
    tests = os.path.join(base, "tests")
    os.makedirs(tests, exist_ok=True)
    with open(os.path.join(tests, "public_checks.cpp"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


def _rule_row(code, d):
    pol = {"strict": "IdPolicy::strict", "keep_blank": "IdPolicy::keep_blank",
           "auto_gen": "IdPolicy::auto_gen"}[d["id_policy"]]
    rep = {"none": "ReplayScope::none", "stage": "ReplayScope::stage",
           "vehicle": "ReplayScope::vehicle", "range": "ReplayScope::range"}[d["replay_scope"]]
    br = {"telemetry": "Branch::telemetry", "command": "Branch::command",
          "config": "Branch::config", "event": "Branch::event"}[d["branch"]]
    bmin, bmax = d["band"][0], d["band"][1]
    return (f"FieldRule{{Structure::{code}, {d['route_code']}, "
            f"static_cast<std::uint8_t>({d['schema_rev']}), {pol}, {rep}, "
            f"{('true' if d['stamp_required'] else 'false')}, "
            f"{('true' if d['ordinal_required'] else 'false')}, "
            f"{d['mode_flag']}u, {d['phase_value']}, {br}, {bmin}, {bmax}}}")


def save_ground_truth(v6_truth, preset_dir):
    base = os.path.join(preset_dir, "q01-long-context-protocol")
    priv = os.path.join(base, "_private")
    os.makedirs(priv, exist_ok=True)
    # ground truth = 每个 code 的 v6 真值(用于评分参考)
    gt = {}
    for code, info in v6_truth.items():
        v = info["vals"]
        gt[code] = {
            "route_code": v["route_code"], "schema_rev": v["schema_rev"],
            "id_policy": v["id_policy"], "replay_scope": v["replay_scope"],
            "stamp_required": v["stamp_required"],
            "ordinal_required": v["ordinal_required"],
            "mode_flag": v["mode_flag"], "phase_value": v["phase_value"],
            "branch": v["branch"], "band": [v["band_min"], v["band_max"]],
        }
    with open(os.path.join(priv, "q01_truth.json"), "w", encoding="utf-8") as f:
        json.dump(gt, f, ensure_ascii=False, indent=2)


def main(preset_dir):
    v6_truth = build_corpus(preset_dir)
    gen_cpp(v6_truth, preset_dir)
    save_ground_truth(v6_truth, preset_dir)
    # 统计语料 token 估算
    _report(preset_dir)


def _report(preset_dir):
    corpus = os.path.join(preset_dir, "q01-long-context-protocol", "corpus")
    total_bytes = 0
    total_chars = 0
    nfiles = 0
    for root, _, files in os.walk(corpus):
        for fn in files:
            p = os.path.join(root, fn)
            with open(p, encoding="utf-8") as f:
                s = f.read()
            total_chars += len(s)
            total_bytes += len(s.encode("utf-8"))
            nfiles += 1
    # 中文为主: token 约等于 字符数/1.6 ~ 字符数/2 (混合中英)。保守按 字符数/1.8。
    est_tokens = int(total_chars / 1.8)
    print(f"corpus 文件数: {nfiles}")
    print(f"总字符数: {total_chars:,}")
    print(f"总字节(UTF-8): {total_bytes:,}")
    print(f"估算 token: ~{est_tokens:,} (目标 ~0.8M / 800,000)")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.pardir)
