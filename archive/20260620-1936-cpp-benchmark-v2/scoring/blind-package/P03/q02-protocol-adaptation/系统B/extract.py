#!/usr/bin/env python3
"""
Extract all System B proto structures with binding info and generate a complete table.
"""

# Data extracted from all .proto files in 系统B directory
# Each entry: (family, structure_name, route_code, schema_rev, id_policy, replay_scope, 
#              stamp_required, ordinal_required, mode_flag, phase, branch, band_min, band_max)

records = [
    # stage family (6)
    ("stage", "stage_state", 765, 5, "keep_blank", "vehicle", True, True, 11, 3, "config", 313, 925),
    ("stage", "stage_states", 924, 7, "keep_blank", "none", True, True, 29, 0, "event", 295, 1082),
    ("stage", "stage_state_view", 960, 7, "keep_blank", "range", False, True, 62, 2, "command", 348, 777),
    ("stage", "stage_state_patch", 694, 5, "auto_gen", "vehicle", False, True, 2, 0, "event", 54, 739),
    ("stage", "stage_state_legacy", 105, 7, "keep_blank", "vehicle", True, False, 33, 1, "event", 389, 498),
    ("stage", "stage_state_v2", 421, 5, "keep_blank", "vehicle", True, False, 23, 3, "telemetry", 0, 112),

    # propulsion family (6)
    ("propulsion", "thrust_report", 709, 8, "keep_blank", "stage", True, True, 1, 1, "event", 362, 486),
    ("propulsion", "thrust_reports", 521, 2, "strict", "none", True, False, 48, 0, "event", 202, 452),
    ("propulsion", "thrust_report_view", 389, 8, "strict", "vehicle", True, False, 33, 5, "event", 30, 435),
    ("propulsion", "thrust_report_patch", 216, 7, "keep_blank", "vehicle", False, False, 16, 0, "config", 333, 1120),
    ("propulsion", "thrust_report_legacy", 242, 9, "auto_gen", "vehicle", True, False, 14, 6, "config", 196, 864),
    ("propulsion", "thrust_report_v2", 453, 2, "keep_blank", "vehicle", True, False, 51, 5, "config", 89, 811),

    # guidance family (6)
    ("guidance", "nav_solution", 700, 2, "auto_gen", "none", True, False, 16, 1, "command", 115, 1083),
    ("guidance", "nav_solutions", 583, 5, "keep_blank", "none", True, True, 10, 0, "command", 158, 406),
    ("guidance", "nav_solution_view", 147, 8, "keep_blank", "none", False, False, 6, 1, "command", 67, 738),
    ("guidance", "nav_solution_patch", 838, 2, "keep_blank", "none", False, True, 3, 5, "event", 419, 805),
    ("guidance", "nav_solution_legacy", 617, 2, "keep_blank", "none", True, True, 54, 1, "command", 241, 796),
    ("guidance", "nav_solution_v2", 701, 8, "keep_blank", "none", True, True, 11, 0, "command", 237, 785),

    # separation family (6)
    ("separation", "sep_event", 786, 5, "auto_gen", "range", False, True, 26, 6, "command", 321, 610),
    ("separation", "sep_events", 625, 8, "auto_gen", "range", True, False, 59, 1, "command", 194, 1044),
    ("separation", "sep_event_view", 867, 6, "auto_gen", "none", True, False, 61, 2, "command", 253, 1129),
    ("separation", "sep_event_patch", 601, 5, "keep_blank", "none", True, True, 30, 2, "config", 31, 311),
    ("separation", "sep_event_legacy", 427, 4, "keep_blank", "vehicle", False, False, 35, 3, "telemetry", 381, 1049),
    ("separation", "sep_event_v2", 644, 6, "strict", "stage", True, False, 43, 1, "event", 482, 1430),

    # rangesafety family (6)
    ("rangesafety", "fts_status", 103, 7, "strict", "none", False, True, 26, 0, "config", 25, 714),
    ("rangesafety", "fts_statuses", 289, 3, "auto_gen", "none", True, False, 53, 5, "event", 17, 885),
    ("rangesafety", "fts_status_view", 915, 4, "auto_gen", "stage", False, True, 34, 2, "telemetry", 223, 497),
    ("rangesafety", "fts_status_patch", 809, 9, "keep_blank", "none", False, True, 7, 5, "command", 389, 955),
    ("rangesafety", "fts_status_legacy", 476, 5, "auto_gen", "vehicle", True, True, 15, 2, "config", 103, 1073),
    ("rangesafety", "fts_status_v2", 429, 5, "auto_gen", "vehicle", True, True, 34, 1, "event", 19, 159),

    # payload family (6)
    ("payload", "payload_frame", 108, 5, "auto_gen", "vehicle", False, True, 45, 5, "telemetry", 363, 994),
    ("payload", "payload_frames", 593, 7, "auto_gen", "range", True, True, 4, 1, "config", 419, 832),
    ("payload", "payload_frame_view", 175, 9, "strict", "vehicle", True, True, 3, 0, "telemetry", 99, 880),
    ("payload", "payload_frame_patch", 697, 5, "keep_blank", "none", True, True, 33, 3, "command", 153, 173),
    ("payload", "payload_frame_legacy", 792, 5, "keep_blank", "stage", True, False, 3, 1, "telemetry", 202, 630),
    ("payload", "payload_frame_v2", 255, 1, "strict", "range", False, True, 31, 0, "event", 36, 463),

    # groundlink family (6)
    ("groundlink", "link_quality", 239, 7, "strict", "range", True, True, 40, 1, "config", 125, 249),
    ("groundlink", "link_qualities", 683, 6, "keep_blank", "vehicle", False, False, 1, 2, "config", 471, 791),
    ("groundlink", "link_quality_view", 509, 9, "strict", "range", False, True, 3, 0, "command", 136, 428),
    ("groundlink", "link_quality_patch", 386, 8, "auto_gen", "none", True, True, 46, 2, "config", 220, 302),
    ("groundlink", "link_quality_legacy", 424, 8, "keep_blank", "vehicle", True, True, 61, 5, "telemetry", 277, 1022),
    ("groundlink", "link_quality_v2", 923, 1, "strict", "stage", True, True, 59, 0, "config", 303, 1110),

    # thermal family (6)
    ("thermal", "thermal_sample", 454, 2, "strict", "range", False, True, 23, 1, "command", 302, 608),
    ("thermal", "thermal_samples", 127, 5, "keep_blank", "vehicle", True, False, 60, 5, "telemetry", 214, 613),
    ("thermal", "thermal_sample_view", 716, 3, "auto_gen", "range", True, False, 59, 0, "config", 330, 629),
    ("thermal", "thermal_sample_patch", 821, 7, "auto_gen", "none", True, True, 2, 3, "telemetry", 447, 1033),
    ("thermal", "thermal_sample_legacy", 749, 4, "strict", "none", False, False, 26, 3, "event", 494, 908),
    ("thermal", "thermal_sample_v2", 316, 3, "strict", "stage", False, True, 2, 2, "event", 121, 234),
]

# Verify count
print(f"Total records: {len(records)}")
print()

# Print header
print("| Family | Structure | route_code | schema_rev | id_policy | replay_scope | stamp_req | ordinal_req | mode_flag | phase | branch | band_min | band_max |")
print("|--------|-----------|------------|------------|-----------|--------------|-----------|-------------|-----------|-------|--------|----------|----------|")

# Print all records
for r in records:
    family, name, rc, sr, ip, rs, st, ord_, mf, ph, br, bmin, bmax = r
    # Format mode_flag as hex
    mf_hex = f"0x{mf:02x}"
    print(f"| {family} | {name} | {rc} | {sr} | {ip} | {rs} | {st} | {ord_} | {mf_hex} ({mf}) | {ph} | {br} | {bmin} | {bmax} |")
