#!/usr/bin/env python3
# coding: utf-8
"""
svg_check.py —— benchmark-v3 / q01 的公开（参评对象可见）SVG 合法性检查。

只校验"产物形似合法 + 标注约定基本合规"，不依赖任何私有 ground truth。
退出码：0 全过，非 0 有失败项。逐项打印 [OK]/[FAIL]。

检查项（每张图）：
  1. well-formed XML（ElementTree 可解析）
  2. 根元素为 <svg>
  3. 可渲染尺寸：有 viewBox，或同时有 width 与 height
  4. 非空：根下至少 2 个子元素
  5. 标注约定基本合规：
       - 存在 data-role="nodes" 与 data-role="edges" 容器
       - 节点数 >= 6（data-id 计数 + <text> 文本兜底）
       - 边数 >= 4（data-from + data-to 成对的 line/path/polyline）
       - 根上有 data-style 且取值合法

目录级检查：diagrams/ 下恰好存在 5 张约定命名的 SVG。
"""

import os
import sys
import xml.etree.ElementTree as ET

SVG_NS = "http://www.w3.org/2000/svg"

EXPECTED_FILES = [
    "01_layered_architecture.svg",
    "02_component_dependency.svg",
    "03_dataflow.svg",
    "04_class_relationship.svg",
    "05_deployment_topology.svg",
]

VALID_STYLES = {"layered", "component", "dataflow", "class", "deployment"}

MIN_NODES = 6
MIN_EDGES = 4


def _local(tag):
    """去掉命名空间前缀，如 '{http://...}svg' -> 'svg'。"""
    if tag and tag.startswith("{"):
        return tag.split("}", 1)[1]
    return tag


def _attr(el, name):
    """取属性，data-* 在 ElementTree 里就是 'data-x'。"""
    return el.get(name)


def _walk(el):
    yield el
    for child in list(el):
        yield from _walk(child)


def check_one(path):
    """返回 (ok: bool, messages: list[str])。"""
    msgs = []
    name = os.path.basename(path)
    try:
        with open(path, "rb") as f:
            tree = ET.parse(f)
    except ET.ParseError as e:
        msgs.append(f"[FAIL] {name}: 非 well-formed XML（{e}）")
        return False, msgs
    except FileNotFoundError:
        msgs.append(f"[FAIL] {name}: 文件不存在")
        return False, msgs

    root = tree.getroot()
    if _local(root.tag) != "svg":
        msgs.append(f"[FAIL] {name}: 根元素不是 <svg>（实际 <{_local(root.tag)}>）")
        return False, msgs
    msgs.append(f"[OK] {name}: well-formed，根为 <svg>")

    # 尺寸
    has_viewbox = _attr(root, "viewBox") is not None
    has_wh = _attr(root, "width") is not None and _attr(root, "height") is not None
    if not (has_viewbox or has_wh):
        msgs.append(f"[FAIL] {name}: 缺少 viewBox，且未同时提供 width/height（无法稳定渲染）")
        return False, msgs
    msgs.append(f"[OK] {name}: 可渲染尺寸（{'viewBox' if has_viewbox else 'width+height'}）")

    # 非空
    children = list(root)
    if len(children) < 2:
        msgs.append(f"[FAIL] {name}: 内容过少（根下仅 {len(children)} 个元素，需 >=2）")
        return False, msgs

    # data-style
    style = _attr(root, "data-style")
    if style is None:
        msgs.append(f"[FAIL] {name}: 根缺少 data-style 属性（应取 {sorted(VALID_STYLES)} 之一）")
        return False, msgs
    if style not in VALID_STYLES:
        msgs.append(f"[FAIL] {name}: data-style='{style}' 非法（应为 {sorted(VALID_STYLES)} 之一）")
        return False, msgs

    # 找 nodes / edges 容器
    nodes_container = None
    edges_container = None
    for el in _walk(root):
        role = _attr(el, "data-role")
        if role == "nodes" and nodes_container is None:
            nodes_container = el
        elif role == "edges" and edges_container is None:
            edges_container = el

    if nodes_container is None or edges_container is None:
        miss = []
        if nodes_container is None:
            miss.append("data-role=\"nodes\"")
        if edges_container is None:
            miss.append("data-role=\"edges\"")
        msgs.append(f"[FAIL] {name}: 缺少标注容器（{', '.join(miss)}）")
        return False, msgs

    # 节点数：data-id 计数；无 data-id 时用 <text> 文本兜底
    node_ids = set()
    for el in _walk(nodes_container):
        nid = _attr(el, "data-id")
        if nid:
            node_ids.add(nid.strip())
    text_count = 0
    for el in _walk(nodes_container):
        if _local(el.tag) == "text" and (el.text or "").strip():
            text_count += 1
    node_count = len(node_ids) if node_ids else text_count
    if node_count < MIN_NODES:
        msgs.append(
            f"[FAIL] {name}: 节点数不足（{node_count} < {MIN_NODES}；"
            f"data-id 命中 {len(node_ids)}，text 兜底 {text_count}）"
        )
        return False, msgs

    # 边数：data-from + data-to 成对
    edges = 0
    for el in _walk(edges_container):
        tag = _local(el.tag)
        if tag in ("line", "path", "polyline", "polygon") and \
           _attr(el, "data-from") and _attr(el, "data-to"):
            edges += 1
    if edges < MIN_EDGES:
        msgs.append(f"[FAIL] {name}: 边数不足（{edges} < {MIN_EDGES}；需 data-from+data-to 的 line/path）")
        return False, msgs

    msgs.append(
        f"[OK] {name}: 标注合规 data-style={style}，节点 {node_count}(>={MIN_NODES})，边 {edges}(>={MIN_EDGES})"
    )
    return True, msgs


def main(diagrams_dir):
    all_ok = True

    # 目录级：5 文件齐全
    actual = set()
    if os.path.isdir(diagrams_dir):
        actual = {n for n in os.listdir(diagrams_dir) if n.endswith(".svg")}
    missing = [f for f in EXPECTED_FILES if f not in actual]
    extra = sorted(actual - set(EXPECTED_FILES))
    if missing:
        all_ok = False
        print(f"[FAIL] 缺少约定的 SVG：{missing}")
    else:
        print(f"[OK] 5 张约定命名的 SVG 齐全")
    if extra:
        print(f"[WARN] 存在非约定命名的 SVG（将被忽略）：{extra}")

    print("-" * 60)
    for fname in EXPECTED_FILES:
        path = os.path.join(diagrams_dir, fname)
        ok, msgs = check_one(path)
        if not ok:
            all_ok = False
        for m in msgs:
            print(m)
        print("-" * 60)

    print("=" * 60)
    if all_ok:
        print("公开 SVG 检查全部通过。")
        return 0
    print("公开 SVG 检查未通过（见上述 [FAIL]）。")
    return 1


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "diagrams"
    sys.exit(main(d))
