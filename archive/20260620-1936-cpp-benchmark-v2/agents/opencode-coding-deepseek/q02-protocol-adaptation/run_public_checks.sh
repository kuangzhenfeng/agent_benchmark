#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binary="${TMPDIR:-/tmp}/协议适配公开检查-$$"
trap 'rm -f "$binary"' EXIT

g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
  -I"$root/include" "$root/src/protocol_adapter.cpp" \
  "$root/tests/public_checks.cpp" -o "$binary"
timeout 5s "$binary"
