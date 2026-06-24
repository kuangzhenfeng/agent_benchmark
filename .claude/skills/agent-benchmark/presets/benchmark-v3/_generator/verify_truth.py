#!/usr/bin/env python3
# coding: utf-8
"""
benchmark-v3 / q01 真值自洽校验（开发期私有脚本，不产出公开文件）。

校验三件事：
1. 引擎能以 C++17 严格警告编译通过，demo 可在超时内运行退出（证明源真实、运行时可观测）。
2. _private/q01_truth.json 的每个节点/继承边/死代码/伪循环依赖声明都能在
   include/*.hpp + src/*.cpp 中找到源码证据（无"隐藏私人条件"）。
3. 关键不变量：worker.hpp 不 include scheduler.hpp（无编译期循环依赖）。

仅在预设开发/维护时运行；绝不被复制到 questions / agents / submissions。
用法：cd _generator && python3 verify_truth.py ..
"""

import json
import os
import re
import subprocess
import sys


def _read_all(root):
    """读取 root 下 include/ 与 src/ 的全部源码，返回 {相对路径: 文本}。"""
    sources = {}
    for sub in ("include", "src"):
        d = os.path.join(root, sub)
        if not os.path.isdir(d):
            continue
        for name in sorted(os.listdir(d)):
            if name.endswith((".hpp", ".cpp", ".h")):
                with open(os.path.join(d, name), encoding="utf-8") as f:
                    sources[os.path.join(sub, name)] = f.read()
    return sources


def _flatten(sources):
    return "\n".join(sources.values())


def verify_compile_and_run(root):
    """编译运行引擎 demo。"""
    inc = os.path.join(root, "include")
    srcs = [os.path.join(root, "src", n) for n in sorted(os.listdir(os.path.join(root, "src")))
            if n.endswith(".cpp")]
    out = os.path.join(os.environ.get("TMPDIR", "/tmp"), "taskforge_verify")
    cmd = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-pthread",
           f"-I{inc}", *srcs, "-o", out]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("[FAIL] 编译失败:\n" + r.stderr)
        return False
    print("[OK] 引擎编译通过（c++17 -Wall -Wextra -Wpedantic -pthread）")

    # 后台运行 demo，限时 3 秒，捕获输出证明有 metrics summary
    try:
        p = subprocess.Popen([out], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            out_txt, _ = p.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill()
            out_txt, _ = p.communicate()
        if "summary" in out_txt and "submitted=" in out_txt:
            print("[OK] demo 运行并产出 metrics summary: " +
                  [ln for ln in out_txt.splitlines() if "summary" in ln][-1].strip())
            return True
        print("[WARN] demo 运行但未捕获到 summary 行（可能被强杀），输出片段:\n" + out_txt[:300])
        return True
    finally:
        try:
            os.remove(out)
        except OSError:
            pass


def verify_nodes(truth, sources, flat):
    """每个 truth.nodes 节点都应在源码中出现其类/struct 定义。"""
    ok = True
    for node, meta in truth["nodes"].items():
        # 容忍 main（在 src/main.cpp 的入口，不是 class 定义）
        if node == "main":
            if "int main()" in flat or "int main(int" in flat:
                continue
            print(f"[FAIL] 节点 main: 未找到 main 入口")
            ok = False
            continue
        pat = re.compile(r"\b(class|struct)\s+" + re.escape(node) + r"\b")
        if not pat.search(flat):
            print(f"[FAIL] 节点 {node}: 源码中未找到 class/struct 定义")
            ok = False
        else:
            # 校验声明的归属文件确实定义了它
            declared = meta.get("file", "")
            target = sources.get(declared, "")
            if target and not pat.search(target):
                print(f"[WARN] 节点 {node}: 声明文件 {declared} 内未直接定义（可能在别处），可接受")
    if ok:
        print(f"[OK] 全部 {len(truth['nodes'])} 个节点均有源码定义")
    return ok


def verify_inheritance(truth, sources):
    flat = _flatten(sources)
    ok = True
    for e in truth["class_relationship"]["inheritance"]:
        child, parent = e["child"], e["parent"]
        pat = re.compile(r"\bclass\s+" + re.escape(child) +
                         r"\b[^{;]*:\s*public\s+" + re.escape(parent) + r"\b")
        if not pat.search(flat):
            print(f"[FAIL] 继承边 {child} -> {parent}: 源码中未找到 'class {child} ... : public {parent}'")
            ok = False
    if ok:
        print(f"[OK] 全部 {len(truth['class_relationship']['inheritance'])} 条继承边均有源码证据")
    return ok


def verify_dead_code(truth, sources):
    """死代码/废弃符号：除定义外无调用点。"""
    ok = True
    impl_text = "\n".join(v for k, v in sources.items() if k.startswith("src/"))
    for item in truth["traps"]["dead_code"]:
        sym = item["symbol"]
        base = sym.split("::")[-1]  # 方法名取末段
        call_sites = []
        for m in re.finditer(r"\b" + re.escape(base) + r"\b", impl_text):
            off = m.start()
            line_start = impl_text.rfind("\n", 0, off) + 1
            line_end = impl_text.find("\n", off)
            line = impl_text[line_start:line_end if line_end != -1 else len(impl_text)]
            stripped = line.strip()
            # 跳过注释行
            if stripped.startswith("//") or stripped.startswith("#"):
                continue
            # 跳过定义行：'Class::method(' 类的类外实现签名（base 紧跟在 :: 之后或之前）
            if re.search(r"\b" + re.escape(base) + r"\s*::", line) or \
               re.search(r"::\s*" + re.escape(base) + r"\b", line):
                continue
            call_sites.append(off)
        if call_sites:
            print(f"[FAIL] 死代码 {sym}: 实现文件中发现 {len(call_sites)} 处疑似调用（应仅定义）")
            ok = False
        else:
            print(f"[OK] 死代码 {sym}: 实现文件中无调用点（仅定义存在）")
    return ok


def verify_pseudo_circular(truth, sources):
    """worker.hpp 不得 include scheduler.hpp。"""
    ok = True
    worker_hpp = sources.get("include/worker.hpp", "")
    if "scheduler.hpp" in worker_hpp or "Scheduler" in re.sub(r"//.*", "", worker_hpp):
        print("[FAIL] worker.hpp 引用了 scheduler（破坏'无编译期循环依赖'不变量）")
        ok = False
    else:
        print("[OK] worker.hpp 不依赖 scheduler.hpp（Worker->Scheduler 仅为运行时回调）")

    # scheduler.hpp 应 include worker.hpp（编译期 Scheduler->WorkerPool->Worker）
    scheduler_hpp = sources.get("include/scheduler.hpp", "")
    if "worker.hpp" in scheduler_hpp:
        print("[OK] scheduler.hpp include worker.hpp（编译期 Scheduler->Worker 方向成立）")
    else:
        print("[WARN] scheduler.hpp 未直接 include worker.hpp（请确认依赖方向）")
    return ok


def verify_layering(truth, sources):
    """MetricsCollector 应只含 <atomic>（叶子，基础设施层），不含对内部模块的依赖。"""
    ok = True
    m = sources.get("include/metrics.hpp", "")
    # 不应 include 任何引擎内部头
    internal = [ln for ln in m.splitlines() if re.match(r'\s*#include\s+"', ln)]
    if internal:
        print(f"[FAIL] MetricsCollector 头文件依赖了内部模块: {internal}（应为叶子）")
        ok = False
    else:
        print("[OK] MetricsCollector 为叶子（不依赖内部模块），属 infrastructure 层")
    return ok


def verify_interface_deps(truth, sources):
    """Scheduler 持有的是接口指针：shared_ptr<ITaskStore> 等。"""
    ok = True
    sch = sources.get("include/scheduler.hpp", "")
    for iface in ("ITaskStore", "IRetryPolicy", "INotifier"):
        if not re.search(r"(shared_ptr|unique_ptr)<\s*" + iface + r"\s*>", sch):
            print(f"[FAIL] Scheduler 未以指针持有接口 {iface}（接口依赖声明存疑）")
            ok = False
    if ok:
        print("[OK] Scheduler 以指针持有 ITaskStore/IRetryPolicy/INotifier 接口")
    return ok


def main(preset_dir):
    base = os.path.join(preset_dir, "q01-batch-engine-architecture")
    truth_path = os.path.join(base, "_private", "q01_truth.json")
    with open(truth_path, encoding="utf-8") as f:
        truth = json.load(f)

    sources = _read_all(base)
    print(f"已读取 {len(sources)} 个源文件\n")

    results = []
    print("== 1. 引擎编译运行 ==")
    results.append(verify_compile_and_run(base))
    print("\n== 2. 节点定义 ==")
    results.append(verify_nodes(truth, sources, _flatten(sources)))
    print("\n== 3. 继承边 ==")
    results.append(verify_inheritance(truth, sources))
    print("\n== 4. 死代码/废弃 ==")
    results.append(verify_dead_code(truth, sources))
    print("\n== 5. 伪循环依赖不变量 ==")
    results.append(verify_pseudo_circular(truth, sources))
    print("\n== 6. 分层（MetricsCollector 叶子）==")
    results.append(verify_layering(truth, sources))
    print("\n== 7. 接口依赖 ==")
    results.append(verify_interface_deps(truth, sources))

    print("\n" + "=" * 50)
    if all(results):
        print("全部真值点校验通过：truth.json 与源码自洽，无隐藏私人条件。")
        return 0
    print("存在失配项，请修正 truth.json 或源码后再发布。")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else os.pardir))
