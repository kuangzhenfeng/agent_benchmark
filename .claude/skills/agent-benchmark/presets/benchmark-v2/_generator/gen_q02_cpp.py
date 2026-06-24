# coding: utf-8
"""
Q02 的 C++ 公开头与 starter 生成器。

生成:
- include/protocol_adapter.hpp: 48 个 Structure 枚举、10 维度 FieldRule、派生校验函数声明。
- src/protocol_adapter.cpp: starter 实现, 规则表填充"系统 A 的已完成值"(迁移前)。
  参评 Agent 必须把它改成"系统 B 的最终值"。
- tests/public_checks.cpp: 抽样检查 8 个绑定结构 + 派生函数语义。

注意: public_checks 抽样检查的是系统 B 的真值(目标)。starter 填的是系统 A 值,
故 starter 阶段 public_checks 必然失败(符合预期: 规则表仍为适配前值)。
"""

import os
import json

import model
from model import (all_bound_codes, gen_field_values, perturb_for_a,
                   ID_POLICY, REPLAY_SCOPE, MODE_BITS, BRANCHES)


HEADER = r'''#pragma once

// 协议适配公开 API(不可修改)。
//
// 系统 A(range.tc)已完成一次结构契约升级, 现需把每个"实现绑定"结构的适配规则,
// 从系统 A 的已完成值迁移到系统 B(orbit.tc)的最终值。本头文件定义结构代号、
// 适配字段维度与派生校验函数的公开契约。

#include <cstdint>
#include <optional>
#include <string_view>

// 48 个实现绑定结构代号。base/plural/view/patch/legacy/v2 六种变体 x 八子系统族。
// 注意: 名称相近不表示字段相同, 逐结构必须独立读取目标值。
enum class Structure {
    // stage 族
    stage_state, stage_states, stage_state_view, stage_state_patch, stage_state_legacy, stage_state_v2,
    // propulsion 族
    thrust_report, thrust_reports, thrust_report_view, thrust_report_patch, thrust_report_legacy, thrust_report_v2,
    // guidance 族
    nav_solution, nav_solutions, nav_solution_view, nav_solution_patch, nav_solution_legacy, nav_solution_v2,
    // separation 族
    sep_event, sep_events, sep_event_view, sep_event_patch, sep_event_legacy, sep_event_v2,
    // rangesafety 族
    fts_status, fts_statuses, fts_status_view, fts_status_patch, fts_status_legacy, fts_status_v2,
    // payload 族
    payload_frame, payload_frames, payload_frame_view, payload_frame_patch, payload_frame_legacy, payload_frame_v2,
    // groundlink 族
    link_quality, link_qualities, link_quality_view, link_quality_patch, link_quality_legacy, link_quality_v2,
    // thermal 族
    thermal_sample, thermal_samples, thermal_sample_view, thermal_sample_patch, thermal_sample_legacy, thermal_sample_v2,
};

enum class IdPolicy   { strict, keep_blank, auto_gen };
enum class ReplayScope{ none, stage, vehicle, range };
enum class Branch     { telemetry, command, config, event };

// 模式位编号(0..5)。逐位语义独立, 不可整体复制或比较。
enum class ModeBit {
    arm = 0, ignite = 1, hold = 2, safe = 3, redundant = 4, debug = 5,
};

// 适配字段规则(10 个互相独立的维度)。
struct FieldRule {
    Structure     id;
    std::uint16_t route_code;        // 路由码
    std::uint8_t  schema_rev;        // 协议小版本
    IdPolicy      id_policy;
    ReplayScope   replay_scope;
    bool          stamp_required;
    bool          ordinal_required;
    std::uint32_t mode_flag;         // 六位模式位(bit0..bit5)
    std::uint8_t  phase_value;       // 飞行阶段编号(系统 A 与系统 B 不同)
    Branch        branch;            // 载荷分支
    std::int32_t  band_min;          // 合法区间下界
    std::int32_t  band_max;          // 合法区间上界
};

// 入站记录(用于 accepts 判定)。
struct Incoming {
    std::string_view identifier;
    bool has_stamp   = false;
    bool has_ordinal = false;
    std::uint32_t mode_flag = 0;
    std::uint8_t  phase_value = 0;
    std::int32_t  sample = 0;        // 待校验样本值(用于 within_band)
};

// 取某结构的规则。未绑定结构返回 nullopt。
std::optional<FieldRule> rule_for(Structure id);

// 派生校验(必须由 rule_for 驱动, 不得按名称/位置/族系生成默认行为):
bool accepts(Structure id, const Incoming& in);            // 标识/时间戳/序号策略
bool replayable(Structure id);                            // 回放域 != none
bool flag_active(Structure id, ModeBit bit);              // 指定模式位是否置位
int  phase_rank(Structure id);                            // 阶段编号(供排序/迁移比较)
Branch branch_of(Structure id);                           // 载荷分支
bool within_band(Structure id, std::int32_t sample);      // 样本是否落在合法区间
'''


def _cpp_rule_row(code, v, enum_name):
    """生成 FieldRule 行。v 是字段值 dict。"""
    pol = {"strict": "IdPolicy::strict", "keep_blank": "IdPolicy::keep_blank",
           "auto_gen": "IdPolicy::auto_gen"}[v["id_policy"]]
    rep = {"none": "ReplayScope::none", "stage": "ReplayScope::stage",
           "vehicle": "ReplayScope::vehicle", "range": "ReplayScope::range"}[v["replay_scope"]]
    br = {"telemetry": "Branch::telemetry", "command": "Branch::command",
          "config": "Branch::config", "event": "Branch::event"}[v["branch"]]
    return (
        f"    {{Structure::{enum_name}, {v['route_code']}, {v['schema_rev']}, "
        f"{pol}, {rep}, {'true' if v['stamp_required'] else 'false'}, "
        f"{'true' if v['ordinal_required'] else 'false'}, "
        f"{v['mode_flag']:#04x}u, {v['phase_value']}, {br}, "
        f"{v['band_min']}, {v['band_max']}}},\n"
    )


def gen_cpp(system, truth, preset_dir):
    """生成 starter。system='A' 产出迁移前值, 评分参考另存系统 B。"""
    codes = all_bound_codes()
    # 枚举名 = 结构代号(合法 C++ 标识符, 已是 snake_case)
    rows = []
    for c in codes:
        rows.append(_cpp_rule_row(c, truth[c], c))
    impl = (
        '#include "protocol_adapter.hpp"\n\n'
        '#include <array>\n\n'
        'namespace {\n\n'
        '// 规则表: 当前为【系统 A 的已完成值】(迁移前)。参评 Agent 须改为系统 B 的最终值。\n'
        f'constexpr std::array<FieldRule, {len(codes)}> kRules{{{{\n'
        + "".join(rows)
        + "}};\n\n"
        "}  // 匿名命名空间\n\n"
        + _derive_impl()
    )
    src_dir = os.path.join(preset_dir, "q02-protocol-adaptation", "src")
    os.makedirs(src_dir, exist_ok=True)
    with open(os.path.join(src_dir, "protocol_adapter.cpp"), "w", encoding="utf-8") as f:
        f.write(impl)


def _derive_impl():
    """派生函数实现(正确版, 不依赖具体规则值)。"""
    return r'''std::optional<FieldRule> rule_for(Structure id) {
    for (const FieldRule& r : kRules) {
        if (r.id == id) return r;
    }
    return std::nullopt;
}

bool accepts(Structure id, const Incoming& in) {
    const auto r = rule_for(id);
    if (!r) return false;
    // 仅"严格必填"拒绝空标识; 保留空值与空则生成均接受。
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
    const std::uint32_t mask = std::uint32_t{1} << static_cast<unsigned>(bit);
    return (r->mode_flag & mask) != 0;
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


def gen_public_checks(truth_b, preset_dir):
    """公开检查: 抽样 8 个绑定结构 + 派生语义, 校验系统 B 真值。"""
    codes = all_bound_codes()
    # 选 8 个抽样(覆盖不同族与变体)
    sample = [codes[0], codes[7], codes[11], codes[16], codes[23], codes[30], codes[37], codes[45]]
    lines = []
    lines.append('#include "protocol_adapter.hpp"\n\n')
    lines.append('#include <cassert>\n#include <cstdint>\n\n')
    lines.append('namespace {\n\n')
    lines.append('void check_one(Structure id, const FieldRule& exp) {\n')
    lines.append('    const auto r = rule_for(id);\n')
    lines.append('    assert(r.has_value());\n')
    lines.append('    assert(r->route_code == exp.route_code);\n')
    lines.append('    assert(r->schema_rev == exp.schema_rev);\n')
    lines.append('    assert(r->id_policy == exp.id_policy);\n')
    lines.append('    assert(r->replay_scope == exp.replay_scope);\n')
    lines.append('    assert(r->stamp_required == exp.stamp_required);\n')
    lines.append('    assert(r->ordinal_required == exp.ordinal_required);\n')
    lines.append('    assert(r->mode_flag == exp.mode_flag);\n')
    lines.append('    assert(r->phase_value == exp.phase_value);\n')
    lines.append('    assert(r->branch == exp.branch);\n')
    lines.append('    assert(r->band_min == exp.band_min);\n')
    lines.append('    assert(r->band_max == exp.band_max);\n')
    lines.append('    // 派生语义\n')
    lines.append('    assert(replayable(id) == (exp.replay_scope != ReplayScope::none));\n')
    lines.append('    assert(phase_rank(id) == exp.phase_value);\n')
    lines.append('    assert(branch_of(id) == exp.branch);\n')
    lines.append('    assert(within_band(id, exp.band_min));\n')
    lines.append('    assert(within_band(id, exp.band_max));\n')
    lines.append('    assert(!within_band(id, exp.band_min - 1));\n')
    lines.append('    assert(!within_band(id, exp.band_max + 1));\n')
    lines.append('}\n\n')
    lines.append('}  // 匿名命名空间\n\n')
    lines.append('int main() {\n')
    for c in sample:
        v = truth_b[c]
        lines.append("    check_one(Structure::" + c + ", " + _rule_row(c, v) + ");\n")
    # 额外: flag_active 抽样
    c0 = sample[0]
    v0 = truth_b[c0]
    lines.append(f"    // flag_active 抽样: {c0} mode_flag={v0['mode_flag']:#04x}\n")
    for i, name in enumerate(MODE_BITS):
        active = bool(v0["mode_flag"] & (1 << i))
        lines.append(
            f"    assert(flag_active(Structure::{c0}, ModeBit::{name}) == "
            f"{'true' if active else 'false'});\n"
        )
    lines.append('    return 0;\n}\n')
    tests_dir = os.path.join(preset_dir, "q02-protocol-adaptation", "tests")
    os.makedirs(tests_dir, exist_ok=True)
    with open(os.path.join(tests_dir, "public_checks.cpp"), "w", encoding="utf-8") as f:
        f.write("".join(lines))


def _rule_row(code, v):
    pol = {"strict": "IdPolicy::strict", "keep_blank": "IdPolicy::keep_blank",
           "auto_gen": "IdPolicy::auto_gen"}[v["id_policy"]]
    rep = {"none": "ReplayScope::none", "stage": "ReplayScope::stage",
           "vehicle": "ReplayScope::vehicle", "range": "ReplayScope::range"}[v["replay_scope"]]
    br = {"telemetry": "Branch::telemetry", "command": "Branch::command",
          "config": "Branch::config", "event": "Branch::event"}[v["branch"]]
    return (
        f"FieldRule{{Structure::{code}, {v['route_code']}, "
        f"static_cast<std::uint8_t>({v['schema_rev']}), {pol}, {rep}, "
        f"{('true' if v['stamp_required'] else 'false')}, "
        f"{('true' if v['ordinal_required'] else 'false')}, "
        f"{v['mode_flag']}u, {v['phase_value']}, {br}, {v['band_min']}, {v['band_max']}}}"
    )


def main(preset_dir):
    base = os.path.join(preset_dir, "q02-protocol-adaptation")
    inc = os.path.join(base, "include")
    os.makedirs(inc, exist_ok=True)
    with open(os.path.join(inc, "protocol_adapter.hpp"), "w", encoding="utf-8") as f:
        f.write(HEADER)

    with open(os.path.join(base, "_private", "q02_truth.json"), encoding="utf-8") as f:
        truth = json.load(f)
    # starter 用系统 A 值(迁移前)
    gen_cpp("A", truth["system_A"], preset_dir)
    # 公开检查用系统 B 真值(目标)
    gen_public_checks(truth["system_B"], preset_dir)
    print("Q02 C++ 头/starter/公开检查已生成")


if __name__ == "__main__":
    import sys
    main(sys.argv[1] if len(sys.argv) > 1 else os.pardir)
