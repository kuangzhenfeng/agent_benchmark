#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binary="${TMPDIR:-/tmp}/subscription-hub-public-checks-$$"
trap 'rm -f "$binary"' EXIT

g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
  -I"$root/include" "$root/src/subscription_hub.cpp" \
  "$root/tests/public_checks.cpp" -o "$binary"
"$binary"
