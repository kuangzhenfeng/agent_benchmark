#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binary="${TMPDIR:-/tmp}/长上下文校验公开检查-$$"
trap 'rm -f "$binary"' EXIT

g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
  -I"$root/include" "$root/src/range_validator.cpp" \
  "$root/tests/public_checks.cpp" -o "$binary"
timeout 10s "$binary"
