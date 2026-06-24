#!/usr/bin/env bash
# benchmark-v3 / q01 公开检查：编译运行引擎 demo + 校验 5 张 SVG。
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
demo="${TMPDIR:-/tmp}/taskforge-demo-$$"
trap 'rm -f "$demo"' EXIT

echo "=== 1. 编译并运行 TaskForge demo ==="
g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I"$root/include" "$root"/src/*.cpp -o "$demo"

# demo 自带停止逻辑，约 1 秒内退出；给 6 秒上限防卡死
"$demo" &
demo_pid=$!
# 等待 demo 自然退出，超时则终止
for _ in $(seq 1 60); do
    if ! kill -0 "$demo_pid" 2>/dev/null; then
        break
    fi
    sleep 0.1
done
if kill -0 "$demo_pid" 2>/dev/null; then
    echo "[WARN] demo 未在预期时间内退出，已终止"
    kill -9 "$demo_pid" 2>/dev/null || true
    wait "$demo_pid" 2>/dev/null || true
else
    wait "$demo_pid" 2>/dev/null || true
fi
echo "demo 运行结束。"

echo
echo "=== 2. 校验 5 张 SVG 的合法性与标注约定 ==="
python3 "$root/svg_check.py" "$root/diagrams"
