# coding: utf-8
"""
Q02 参考验证: 生成"正确解答"(系统 B 值的 .cpp), 临时编译跑通 public_checks。
用于确认生成器产出的 B 真值自洽、C++ 能编译通过、公开检查全绿。
此脚本只在开发期使用, 不产出公开文件。
"""
import os
import sys
import subprocess
import tempfile

import model
from model import all_bound_codes, gen_field_values, perturb_for_a
import gen_q02_cpp


def main(preset_dir):
    base = os.path.join(preset_dir, "q02-protocol-adaptation")
    import json
    with open(os.path.join(base, "_private", "q02_truth.json"), encoding="utf-8") as f:
        truth = json.load(f)
    truth_b = truth["system_B"]
    # 用系统 B 值生成"正确"的 cpp 内容
    codes = all_bound_codes()
    rows = []
    for c in codes:
        rows.append(gen_q02_cpp._cpp_rule_row(c, truth_b[c], c))
    impl = (
        '#include "protocol_adapter.hpp"\n\n#include <array>\n\nnamespace {\n\n'
        f'constexpr std::array<FieldRule, {len(codes)}> kRules{{{{\n'
        + "".join(rows) + "}};\n\n}  // 匿名命名空间\n\n" + gen_q02_cpp._derive_impl()
    )
    tmpdir = tempfile.mkdtemp()
    open(os.path.join(tmpdir, "good.cpp"), "w", encoding="utf-8").write(impl)
    # 编译 good.cpp + public_checks
    out = os.path.join(tmpdir, "good")
    r = subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-pthread",
         f"-I{os.path.join(base,'include')}",
         os.path.join(tmpdir, "good.cpp"),
         os.path.join(base, "tests", "public_checks.cpp"), "-o", out],
        capture_output=True, text=True)
    print("compile:", r.returncode)
    if r.stderr:
        print(r.stderr)
    r2 = subprocess.run([out], capture_output=True, text=True)
    print("run:", r2.returncode, "(0 = 全绿, 正确解通过所有公开检查)")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.pardir)
