#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binaries=()
trap 'rm -f "${binaries[@]}"' EXIT

run_check() {
  local source="$1"
  local binary="${TMPDIR:-/tmp}/benefit-allocation-${source##*/}-$$"
  binaries+=("$binary")

  printf 'Running %s\n' "$source"
  g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I"$root/include" "$root/src/benefit_allocation.cpp" \
    "$root/tests/$source" -o "$binary"
  timeout 5s "$binary"
}

run_check public_checks.cpp
run_check audit_and_retry_checks.cpp
