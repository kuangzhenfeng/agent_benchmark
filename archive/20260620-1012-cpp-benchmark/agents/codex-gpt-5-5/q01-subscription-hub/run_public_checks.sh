#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binaries=()
trap 'rm -f "${binaries[@]}"' EXIT

run_check() {
  local source="$1"
  local binary="${TMPDIR:-/tmp}/subscription-hub-${source##*/}-$$"
  binaries+=("$binary")

  printf 'Running %s\n' "$source"
  g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I"$root/include" "$root/src/subscription_hub.cpp" \
    "$root/tests/$source" -o "$binary"
  timeout 5s "$binary"
}

run_check public_checks.cpp
run_check callable_lifetime_checks.cpp
run_check edge_semantics_checks.cpp
