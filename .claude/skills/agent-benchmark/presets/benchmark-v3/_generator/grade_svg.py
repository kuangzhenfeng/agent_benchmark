#!/usr/bin/env python3
# coding: utf-8
"""
grade_svg.py —— benchmark-v3 / q01 的机器评分脚本（私有，评分期使用）。

由主 agent 在全部作答结束后运行：解析每个匿名参评对象提交的 5 张 SVG，
提取其 data-* 标注的节点/边，与私有 ground truth（_private/q01_truth.json）
做集合匹配，输出 machine-grade.json，供匿名 scorer 直接引用"正确性"维度。

不替代 scorer 的主观维度（SVG 产物质量、可维护性、验证证据），只把"正确性"
从主观压成客观的架构事实覆盖率。

用法：
    python3 grade_svg.py <truth.json> <blind_package_dir> [--out machine-grade.json]

blind_package_dir 下每个 Pxx/q01-batch-engine-architecture/diagrams/*.svg 是一份提交。
若直接对 preset 自测，也可传单个 diagrams 目录：
    python3 grade_svg.py ../q01-batch-engine-architecture/_private/q01_truth.json \
        ../q01-batch-engine-architecture/diagrams --single
"""

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET

STYLE_TO_VIEW = {
    "layered": "layered",
    "component": "component_dependency",
    "dataflow": "dataflow",
    "class": "class_relationship",
    "deployment": "deployment",
}

EXPECTED_FILES = [
    "01_layered_architecture.svg",
    "02_component_dependency.svg",
    "03_dataflow.svg",
    "04_class_relationship.svg",
    "05_deployment_topology.svg",
]

# 视角权重（正确性 45 分内部分配）
VIEW_WEIGHTS = {
    "layered": 0.18,
    "component_dependency": 0.22,
    "dataflow": 0.20,
    "class_relationship": 0.25,
    "deployment": 0.15,
}

# 无向边的 kind 集合：匹配时忽略方向
UNDIRECTED_KINDS = {"contains", "composes", "aggregates", "connects", "runs-on"}

VIEW_TRAP_KEY = {
    "layered": "layering_correct",
    "component_dependency": "no_compile_cycle",
    "class_relationship": "interface_deps",
}

TRAP_TOTAL = 3.0  # 陷阱加分的总额度（封顶，乘进 45）


def _local(tag):
    if tag and tag.startswith("{"):
        return tag.split("}", 1)[1]
    return tag


def _walk(el):
    yield el
    for child in list(el):
        yield from _walk(child)


def build_alias_index(truth):
    """构造 {归一化别名: 标准节点名}。标准名本身也纳入。"""
    idx = {}
    for canonical, aliases in truth["aliases"].items():
        idx[_norm(canonical)] = canonical
        for a in aliases:
            idx[_norm(a)] = canonical
    # 节点表里的所有 key 也作标准名
    for n in truth["nodes"]:
        idx[_norm(n)] = n
    return idx


def _norm(s):
    """归一化：小写、去空格/标点/连字符。"""
    if s is None:
        return ""
    s = s.strip().lower()
    out = []
    for ch in s:
        if ch.isalnum():
            out.append(ch)
    return "".join(out)


def _resolve(name, alias_index):
    return alias_index.get(_norm(name))


def parse_svg(path, alias_index):
    """解析一张 SVG，返回 (style, nodes:set[canonical], edges:set[(from,to,kind_norm)])。"""
    try:
        tree = ET.parse(path)
    except (ET.ParseError, FileNotFoundError):
        return None, set(), set()
    root = tree.getroot()
    style = root.get("data-style")

    nodes_container = None
    edges_container = None
    for el in _walk(root):
        if el.get("data-role") == "nodes" and nodes_container is None:
            nodes_container = el
        elif el.get("data-role") == "edges" and edges_container is None:
            edges_container = el

    nodes = set()
    if nodes_container is not None:
        for el in _walk(nodes_container):
            nid = el.get("data-id")
            if nid:
                canon = _resolve(nid, alias_index)
                nodes.add(canon if canon else _norm(nid))
            elif _local(el.tag) == "text" and (el.text or "").strip():
                # text 兜底
                canon = _resolve(el.text.strip(), alias_index)
                if canon:
                    nodes.add(canon)

    edges = set()
    if edges_container is not None:
        for el in _walk(edges_container):
            tag = _local(el.tag)
            if tag in ("line", "path", "polyline", "polygon"):
                frm = el.get("data-from")
                to = el.get("data-to")
                kind = (el.get("data-kind") or "").strip().lower()
                if frm and to:
                    cf = _resolve(frm, alias_index) or _norm(frm)
                    ct = _resolve(to, alias_index) or _norm(to)
                    if kind in UNDIRECTED_KINDS:
                        edges.add(tuple(sorted((cf, ct))) + (kind,))
                    else:
                        edges.add((cf, ct, kind))
    return style, nodes, edges


def truth_view(truth, view):
    """从 truth 取某视角的 (node_set, edge_set)。"""
    block = truth[view]
    tnodes = set()
    tedges = set()
    if view == "layered":
        # layered 用 layers 列成员；节点 = 所有层成员 ∪ 边端点
        for layer in block["layers"]:
            tnodes.update(layer["members"])
        for e in block["edges"]:
            f, t, k = e["from"], e["to"], e.get("kind", "")
            tnodes.add(f); tnodes.add(t)
            if k in UNDIRECTED_KINDS:
                tedges.add(tuple(sorted((f, t))) + (k,))
            else:
                tedges.add((f, t, k))
    elif view in ("component_dependency", "dataflow"):
        tnodes = set(block["nodes"])
        for e in block["edges"]:
            f, t, k = e["from"], e["to"], e.get("kind", "")
            tnodes.add(f); tnodes.add(t)
            if k in UNDIRECTED_KINDS:
                tedges.add(tuple(sorted((f, t))) + (k,))
            else:
                tedges.add((f, t, k))
    elif view == "class_relationship":
        # 节点 = 继承/组合/依赖涉及的全体；边按类别归一 kind
        rels = []
        tnodes = set()
        for e in block["inheritance"]:
            tnodes.add(e["child"]); tnodes.add(e["parent"])
            rels.append((e["child"], e["parent"], "inherits"))
        for e in block["composition"]:
            tnodes.add(e["from"]); tnodes.add(e["to"])
            k = e.get("kind", "composes")
            if k in UNDIRECTED_KINDS:
                rels.append(tuple(sorted((e["from"], e["to"]))) + (k,))
            else:
                rels.append((e["from"], e["to"], k))
        for e in block["dependency"]:
            tnodes.add(e["from"]); tnodes.add(e["to"])
            rels.append((e["from"], e["to"], e.get("kind", "depends")))
        tedges = set(rels)
    elif view == "deployment":
        tnodes = set(inst["id"] for inst in block["instances"])
        rels = []
        for c in block.get("containment", []):
            rels.append(tuple(sorted((c["parent"], c["child"]))) + (c.get("kind", "contains"),))
        for c in block.get("connections", []):
            k = c.get("kind", "connects")
            if k in UNDIRECTED_KINDS:
                rels.append(tuple(sorted((c["from"], c["to"]))) + (k,))
            else:
                rels.append((c["from"], c["to"], k))
        tedges = set(rels)
    return tnodes, tedges


def prf(submitted, reference):
    """集合 precision/recall/f1。"""
    if not submitted and not reference:
        return 1.0, 1.0, 1.0
    tp = len(submitted & reference)
    prec = tp / len(submitted) if submitted else 0.0
    rec = tp / len(reference) if reference else 0.0
    f1 = (2 * prec * rec / (prec + rec)) if (prec + rec) > 0 else 0.0
    return prec, rec, f1


def grade_submission(truth, alias_index, diagrams_dir):
    """对一个提交的 diagrams 目录评分。"""
    views = {}
    for fname, style_key in zip(EXPECTED_FILES, STYLE_TO_VIEW.keys()):
        path = os.path.join(diagrams_dir, fname)
        style, nodes, edges = parse_svg(path, alias_index)
        view = STYLE_TO_VIEW.get(style or style_key)
        # 如果 data-style 与文件名不一致，按 data-style 归位视角；缺失则按文件名默认
        if view is None:
            view = STYLE_TO_VIEW[style_key]

        tnodes, tedges = truth_view(truth, view)
        n_p, n_r, n_f1 = prf(nodes, tnodes)
        e_p, e_r, e_f1 = prf(edges, tedges)
        # 边要求方向/kind 正确：直接用边集合 f1（已含方向语义）
        views[view] = {
            "file": fname,
            "data_style": style,
            "node_precision": round(n_p, 3),
            "node_recall": round(n_r, 3),
            "edge_precision": round(e_p, 3),
            "edge_recall": round(e_r, 3),
            "edge_f1": round(e_f1, 3),
            # 视角得分 = 0.6*节点f1 + 0.4*边f1
            "view_score": round(0.6 * n_f1 + 0.4 * e_f1, 3),
        }

    # 加权覆盖率（折算到 45 分前）
    weighted = sum(views[v]["view_score"] * VIEW_WEIGHTS[v] for v in VIEW_WEIGHTS)
    coverage = round(weighted * 45, 2)

    # 陷阱判定
    trap_flags = {}
    # 1) 分层：MetricsCollector 不应出现在 orchestration 层节点里（按 truth 它属 infrastructure）
    layered_nodes = set()
    _s, layered_nodes, _e = parse_svg(os.path.join(diagrams_dir, EXPECTED_FILES[0]), alias_index)
    # 宽松：只要提交里有分层结构即可，MetricsCollector 归入 infra 由 scorer 复核
    trap_flags["layering_present"] = layered_nodes is not None and len(layered_nodes) >= 6

    # 2) 伪循环：组件依赖图里不应出现 (Scheduler, Worker, callback) 的同时
    #    还存在 (Scheduler, Worker, depends/compile) 的双向编译期边。
    _s2, _cn, comp_edges = parse_svg(os.path.join(diagrams_dir, EXPECTED_FILES[1]), alias_index)
    fwd = ("Scheduler", "Worker")
    has_compile_cycle = False
    for e in comp_edges:
        if e[2] in ("depends", "compile", "uses") and (e[0], e[1]) == fwd:
            # 反向 compile-time 依赖才算循环（运行时 callback 不算）
            if any(e2[2] in ("depends", "compile", "uses") and (e2[0], e2[1]) == (fwd[1], fwd[0])
                   for e2 in comp_edges):
                has_compile_cycle = True
    trap_flags["no_compile_cycle"] = not has_compile_cycle

    # 3) 接口依赖：类关系图里 Scheduler 的依赖边终点应为 ITaskStore/IRetryPolicy/INotifier
    _s3, _cn2, class_edges = parse_svg(os.path.join(diagrams_dir, EXPECTED_FILES[3]), alias_index)
    deps_from_scheduler = {e[1] for e in class_edges if e[0] == "Scheduler"}
    expected_ifaces = {"ITaskStore", "IRetryPolicy", "INotifier"}
    concrete_ifaces = {"InMemoryTaskStore", "PersistentTaskStore",
                       "FixedBackoffPolicy", "ExponentialBackoffPolicy",
                       "ConsoleNotifier", "CallbackNotifier", "CompositeNotifier"}
    interface_deps = bool(expected_ifaces & deps_from_scheduler) and \
        not (concrete_ifaces & deps_from_scheduler)
    trap_flags["interface_deps"] = interface_deps

    # 陷阱加分：每个命中 +1，封顶 TRAP_TOTAL，乘进 45 的占比由 scorer 参照
    trap_hits = sum([
        1 if trap_flags.get("layering_present") else 0,
        1 if trap_flags.get("no_compile_cycle") else 0,
        1 if trap_flags.get("interface_deps") else 0,
    ])
    trap_bonus = round(min(trap_hits, TRAP_TOTAL) / TRAP_TOTAL * 3.0, 2)  # 最多 +3 分

    return {
        "views": views,
        "coverage_score_out_of_45": min(45.0, round(coverage + trap_bonus, 2)),
        "coverage_raw_out_of_45": coverage,
        "trap_bonus": trap_bonus,
        "trap_flags": trap_flags,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("truth")
    ap.add_argument("target", help="blind_package 目录（含 Pxx/...）或单个 diagrams 目录（--single）")
    ap.add_argument("--single", action="store_true", help="target 是单个 diagrams 目录")
    ap.add_argument("--out", default="machine-grade.json")
    args = ap.parse_args()

    with open(args.truth, encoding="utf-8") as f:
        truth = json.load(f)
    alias_index = build_alias_index(truth)

    results = {}
    if args.single:
        results["SAMPLE"] = grade_submission(truth, alias_index, args.target)
    else:
        for entry in sorted(os.listdir(args.target)):
            pdir = os.path.join(args.target, entry)
            if not (entry.startswith("P") and os.path.isdir(pdir)):
                continue
            # 定位 diagrams：Pxx/q01-batch-engine-architecture/diagrams
            d = None
            for root, dirs, files in os.walk(pdir):
                if os.path.basename(root) == "diagrams":
                    d = root
                    break
            if d is None:
                results[entry] = {"error": "未找到 diagrams 目录"}
                continue
            results[entry] = grade_submission(truth, alias_index, d)

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    print(f"已写出 {args.out}，共 {len(results)} 份提交。")
    # 摘要
    for k, v in results.items():
        if "coverage_score_out_of_45" in v:
            print(f"  {k}: 覆盖率 {v['coverage_score_out_of_45']}/45 "
                  f"(raw {v['coverage_raw_out_of_45']}, trap +{v['trap_bonus']}, "
                  f"flags {v['trap_flags']})")
        else:
            print(f"  {k}: {v}")


if __name__ == "__main__":
    main()
