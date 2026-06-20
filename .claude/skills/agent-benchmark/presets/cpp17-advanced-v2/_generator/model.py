# coding: utf-8
"""
range.tc / orbit.tc 协议家族共享模型。

本文件是私有生成器的一部分,不进入公开题源。它定义:
- 仿 tsl 的物模型四象限骨架(Config/Status/Action/Event)与按位 flag 形态;
- 每个实现绑定结构的 10 个独立适配字段维度(契约元数据);
- 系统 A(range.tc)与系统 B(orbit.tc)的 enum 编号差异(核心陷阱);
- 确定性的字段值生成,保证语料与 ground truth 自洽且可复现。

业务领域为运载火箭箭载测控,与 tsl 的近地农业 UAV 物模型无内容重叠。
命名全部去 tsl 血统: 无 .p 后缀、无倒置公司域名、无 uav_p_ 前缀。
"""

import random

# 固定种子: 保证语料与评分参考完全可复现。
SEED = 20260620

# ---------------------------------------------------------------------------
# 子系统族与变体
# ---------------------------------------------------------------------------

# 八个测控子系统族。每族对应一个核心结构名词,模拟真实物模型的子系统划分。
# 注意刻意远离 tsl 的喷洒/航点/植保等农业 UAV 语义。
FAMILIES = {
    "stage": "stage_state",        # 级间状态
    "propulsion": "thrust_report",  # 推力报告
    "guidance": "nav_solution",     # 制导解算
    "separation": "sep_event",      # 级间分离事件
    "rangesafety": "fts_status",    # 飞行终止(安控)状态
    "payload": "payload_frame",     # 载荷数据帧
    "groundlink": "link_quality",   # 地面链路质量
    "thermal": "thermal_sample",    # 热控采样
}

# 六种结构变体。base/plural/view/patch 对应原 q03 的单复视图补丁;
# legacy/v2 是本次加大难度新增的两个"历史包袱"变体,制造近似命名陷阱。
VARIANTS = [
    ("", "base"),       # 单数基线
    ("s", "plural"),    # 复数批量
    ("_view", "view"),  # 只读视图
    ("_patch", "patch"),  # 增量补丁
    ("_legacy", "legacy"),  # 历史遗留
    ("_v2", "v2"),      # 新一代
]


# 六种结构变体。base/plural/view/patch 对应原 q03 的单复视图补丁;
# legacy/v2 是本次加大难度新增的两个"历史包袱"变体,制造近似命名陷阱。
# 每族提供符合英文构词法的复数形式,避免 fts_statuss 这类丑复数。
PLURAL_OVERRIDES = {
    "stage_state": "stage_states",
    "thrust_report": "thrust_reports",
    "nav_solution": "nav_solutions",
    "sep_event": "sep_events",
    "fts_status": "fts_statuses",     # status -> statuses
    "payload_frame": "payload_frames",
    "link_quality": "link_qualities",  # quality -> qualities
    "thermal_sample": "thermal_samples",
}


def _plural_of(core):
    return PLURAL_OVERRIDES.get(core, core + "s")


def structure_code(family, variant_suffix):
    """生成结构代号,如 stage_state / stage_states / stage_state_view。"""
    core = FAMILIES[family]
    if variant_suffix == "s":
        return _plural_of(core)
    return core + variant_suffix


# 48 个实现绑定结构 = 8 族 x 6 变体。
def all_bound_codes():
    codes = []
    for fam in FAMILIES:
        for suffix, _vname in VARIANTS:
            codes.append(structure_code(fam, suffix))
    return codes


# ---------------------------------------------------------------------------
# 适配字段维度(10 个,互相独立)
# ---------------------------------------------------------------------------

# 业务术语 -> 内部枚举索引
ID_POLICY = ["strict", "keep_blank", "auto_gen"]          # 标识策略
ID_POLICY_CN = {"strict": "严格必填", "keep_blank": "保留空",
                "auto_gen": "空则生成"}

REPLAY_SCOPE = ["none", "stage", "vehicle", "range"]      # 回放域
REPLAY_SCOPE_CN = {"none": "不回放", "stage": "本级",
                   "vehicle": "本箭", "range": "本靶区"}

# mode_flag 六位,逐位独立语义。C++ 校验 flag_active(id, bit) 逐位判断。
MODE_BITS = ["arm", "ignite", "hold", "safe", "redundant", "debug"]
MODE_BITS_CN = {"arm": "待发", "ignite": "点火", "hold": "保持",
                "safe": "安控安全", "redundant": "冗余", "debug": "调试"}

# 载荷分支(对应四象限载荷)
BRANCHES = ["telemetry", "command", "config", "event"]
BRANCHES_CN = {"telemetry": "遥测", "command": "指令",
               "config": "配置", "event": "事件"}

# PHASE 飞行阶段。系统 A 与系统 B 的命名与编号故意不同(核心陷阱之一):
# - 同义阶段在不同系统编号不同(coast: A=4 / B=5;insertion: A=5 / B=6);
# - 部分阶段名不同(A 的 ready/liftoff 对应 B 的 standby/boost);
# - B 在 4 号位插入一个 reserved 占位,把后续编号整体后移。
PHASE_A = [("idle", 0), ("ready", 1), ("liftoff", 2),
           ("ascent", 3), ("coast", 4), ("insertion", 5)]
# B 的语义序: 0..5 与 A 对齐;但数值通过 phase_value 映射到不同编号。
PHASE_B_SEMANTIC = ["idle", "standby", "boost", "ascent", "coast", "insertion"]
PHASE_B_NUMBER = [0, 1, 2, 3, 5, 6]  # A 对应语义序在 B 中的真实编号(跳过 4=reserved)


def phase_value(system, idx):
    """按系统返回第 idx 个语义阶段的数值。idx 是语义序(0..5),两系统对齐。"""
    if system == "A":
        return PHASE_A[idx][1]
    return PHASE_B_NUMBER[idx]


def phase_name(system, idx):
    """按系统返回第 idx 个语义阶段的名称。"""
    if system == "A":
        return PHASE_A[idx][0]
    return PHASE_B_SEMANTIC[idx]


# ---------------------------------------------------------------------------
# 确定性字段值生成
# ---------------------------------------------------------------------------

def _rng(*parts):
    """按若干字符串分片构造独立可复现随机源。"""
    return random.Random(":".join(str(p) for p in (SEED, *parts)))


def gen_field_values(system, code):
    """为一个结构在指定系统下生成 10 个适配字段的真值(确定性)。

    system: "A" 或 "B"。
    返回 dict,键见 KEYS。
    """
    r = _rng(system, code)
    route_code = r.randint(100, 999)
    schema_rev = r.randint(1, 9)
    id_policy = r.choice(ID_POLICY)
    replay_scope = r.choice(REPLAY_SCOPE)
    stamp_required = r.choice([True, False])
    ordinal_required = r.choice([True, False])
    # mode_flag: 至少置一位、至多五位,避免全 0/全 1 的退化。
    mode_flag = 0
    nbits = r.randint(1, 5)
    chosen = r.sample(range(6), nbits)
    for b in chosen:
        mode_flag |= (1 << b)
    phase_idx = r.randint(0, 5)
    branch_idx = r.randint(0, 3)
    band_min = r.randint(0, 500)
    band_max = band_min + r.randint(1, 1000)
    return {
        "route_code": route_code,
        "schema_rev": schema_rev,
        "id_policy": id_policy,
        "replay_scope": replay_scope,
        "stamp_required": stamp_required,
        "ordinal_required": ordinal_required,
        "mode_flag": mode_flag,
        "phase_idx": phase_idx,
        "branch_idx": branch_idx,
        "band_min": band_min,
        "band_max": band_max,
        # 派生(渲染时用):
        "phase_value": phase_value(system, phase_idx),
        "branch": BRANCHES[branch_idx],
    }


def perturb_for_a(b_vals, code):
    """从系统 B 真值生成系统 A 的"已完成变更"值。

    设计目标: A 与 B 逐字段以较高概率不同,使"把 A 值机械复制到 B"必然错误,
    且无字段间相关性(必须逐字段独立读取 B)。返回的是独立的 A 真值。
    """
    a = dict(b_vals)
    r = _rng("A-from-B", code)
    # route_code: 多数不同
    if r.random() < 0.85:
        a["route_code"] = r.randint(100, 999)
    if r.random() < 0.8:
        a["schema_rev"] = r.randint(1, 9)
    if r.random() < 0.75:
        a["id_policy"] = r.choice([x for x in ID_POLICY if x != b_vals["id_policy"]] or ID_POLICY)
    if r.random() < 0.75:
        a["replay_scope"] = r.choice([x for x in REPLAY_SCOPE if x != b_vals["replay_scope"]] or REPLAY_SCOPE)
    if r.random() < 0.7:
        a["stamp_required"] = not b_vals["stamp_required"]
    if r.random() < 0.7:
        a["ordinal_required"] = not b_vals["ordinal_required"]
    if r.random() < 0.8:
        mf = 0
        nbits = r.randint(1, 5)
        for b in r.sample(range(6), nbits):
            mf |= (1 << b)
        a["mode_flag"] = mf
    if r.random() < 0.8:
        a["phase_idx"] = r.randint(0, 5)
    if r.random() < 0.75:
        a["branch_idx"] = r.choice([x for x in range(4) if x != b_vals["branch_idx"]] or [0])
    if r.random() < 0.8:
        bm = r.randint(0, 500)
        a["band_min"] = bm
        a["band_max"] = bm + r.randint(1, 1000)
    # 重算 A 的派生
    a["phase_value"] = phase_value("A", a["phase_idx"])
    a["branch"] = BRANCHES[a["branch_idx"]]
    return a


# ---------------------------------------------------------------------------
# 物模型上下文字段(噪声,让 message 像真物模型)
# ---------------------------------------------------------------------------

# 每族的真实物模型字段示例,仅作上下文,不参与适配契约。
FAMILY_PROTO_FIELDS = {
    "stage": [
        ("uint32", "stage_no", "级序号"),
        ("uint32", "stage_flag", "级状态位"),
        ("double", "burn_time_ms", "已工作时间"),
        ("double", "residual_propellant_kg", "剩余推进剂"),
        ("bool", "separated", "是否已分离"),
    ],
    "propulsion": [
        ("double", "actual_thrust_kn", "实际推力"),
        ("double", "chamber_pressure_mpa", "燃烧室压强"),
        ("uint32", "engine_mask", "发动机在位掩码"),
        ("double", "isp_s", "比冲"),
        ("bool", "healthy", "推进健康"),
    ],
    "guidance": [
        ("double", "longitude", "经度"),
        ("double", "latitude", "纬度"),
        ("double", "altitude_m", "海拔"),
        ("double", "velocity_ms", "速度"),
        ("double", "gamma_deg", "弹道倾角"),
    ],
    "separation": [
        ("uint32", "sep_index", "分离序"),
        ("double", "sep_velocity_ms", "分离速度"),
        ("bool", "latched", "锁紧状态"),
        ("double", "clearance_m", "分离间距"),
        ("uint32", "sep_event_id", "分离事件编号"),
    ],
    "rangesafety": [
        ("bool", "armed", "安控解除保险"),
        ("bool", "terminate_requested", "终止请求"),
        ("uint32", "violation_mask", "超限掩码"),
        ("double", "margin", "安全余量"),
        ("bool", "link_alive", "安控链路存活"),
    ],
    "payload": [
        ("uint32", "payload_id", "载荷编号"),
        ("uint32", "frame_seq", "帧序号"),
        ("bytes", "raw", "原始数据"),
        ("bool", "valid", "数据有效"),
        ("double", "temperature_c", "载荷温度"),
    ],
    "groundlink": [
        ("double", "rssi_dbm", "信号强度"),
        ("double", "ber", "误码率"),
        ("uint32", "snr_db", "信噪比"),
        ("uint32", "retransmit_count", "重传计数"),
        ("bool", "locked", "载波锁定"),
    ],
    "thermal": [
        ("double", "skin_temp_c", "蒙皮温度"),
        ("double", "bay_temp_c", "舱内温度"),
        ("uint32", "heater_mask", "加热器掩码"),
        ("double", "gradient", "温度梯度"),
        ("bool", "overtemp", "过温"),
    ],
}
