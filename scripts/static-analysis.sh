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
    echo 'No C++ source files exist yet; clang-tidy and Cppcheck are deferred.'
    exit 0
fi

clang-tidy -p build/dev --config-file=.clang-tidy --extra-arg=-std=c++20 "${sources[@]}"
cppcheck --project=build/dev/compile_commands.json --file-filter=prismatic-launcher/src/nexus/* \
    --file-filter=prismatic-launcher/src/main.cpp --file-filter=tests/nexus/* $(< cppcheck.cfg)
