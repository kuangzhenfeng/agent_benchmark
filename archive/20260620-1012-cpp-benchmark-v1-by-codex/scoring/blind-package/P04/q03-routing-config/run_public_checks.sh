#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binaries=()
trap 'rm -f "${binaries[@]}"' EXIT

run_check() {
  local source="$1"
  local binary="${TMPDIR:-/tmp}/routing-config-${source##*/}-$$"
  binaries+=("$binary")

  printf 'Running %s\n' "$source"
  g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I"$root/include" "$root/src/routing_config.cpp" \
    "$root/tests/$source" -o "$binary"
  timeout 5s "$binary"
}

run_asan_check() {
  local source="$1"
  local binary="${TMPDIR:-/tmp}/routing-config-${source##*/}-$$"
  binaries+=("$binary")

  printf 'Running %s with AddressSanitizer\n' "$source"
  g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -fsanitize=address -fno-omit-frame-pointer \
    -I"$root/include" "$root/src/routing_config.cpp" \
    "$root/tests/$source" -o "$binary"
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 timeout 5s "$binary"
}

run_check public_checks.cpp
run_asan_check cross_instance_current_checks.cpp
run_check observer_copy_reentrancy_checks.cpp
run_check extra_checks.cpp
