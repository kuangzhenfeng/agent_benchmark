// range.tc v6 校验器公开 API(不可修改)。
//
// 本头文件为公开题源, 参评 Agent 不得修改。它定义结构代号、适配字段维度与派生校验。
// 实现请写在 src/range_validator.cpp。
//
// 业务背景见 QUESTION.md 与 corpus/。规则表的正确值为 range.tc v6 真值, 散落在 corpus 多份文档中, 须按 ADR-009 优先级取舍。

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// 48 个实现绑定结构代号。
// base/plural/view/patch/legacy/v2 六种变体 x 八子系统族。
// 注意: 名称相近不表示字段相同; 逐结构必须独立读取目标值。
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

enum class IdPolicy    { strict, keep_blank, auto_gen };
enum class ReplayScope { none, stage, vehicle, range };
enum class Branch      { telemetry, command, config, event };

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
    std::uint8_t  phase_value;       // 飞行阶段编号
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
int  phase_rank(Structure id);                            // 阶段编号(供迁移比较)
Branch branch_of(Structure id);                           // 载荷分支
bool within_band(Structure id, std::int32_t sample);      // 样本是否落在合法区间
