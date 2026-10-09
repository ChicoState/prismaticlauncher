#!/usr/bin/env bash
set -euo pipefail

mapfile -d '' sources < <(
    find prismatic-launcher/src/nexus tests/nexus -type f \
        \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.h' -o -name '*.hpp' \) -print0 2>/dev/null || true
)

if [[ -f prismatic-launcher/src/main.cpp ]]; then
    sources+=("prismatic-launcher/src/main.cpp")
fi

if ((${#sources[@]} == 0)); then
    echo 'No C++ source files exist yet; clang-format check is deferred.'
    exit 0
fi

clang-format --dry-run --Werror "${sources[@]}"
