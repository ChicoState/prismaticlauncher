#!/usr/bin/env bash
set -euo pipefail

mapfile -d '' sources < <(find src tests -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.h' -o -name '*.hpp' \) -print0 2>/dev/null || true)

if ((${#sources[@]} == 0)); then
    echo 'No C++ source files exist yet; clang-tidy and Cppcheck are deferred.'
    exit 0
fi

clang-tidy --config-file=.clang-tidy --extra-arg=-std=c++20 "${sources[@]}"
cppcheck --project=build/dev/compile_commands.json --file-filter=src/* --file-filter=tests/* $(< cppcheck.cfg)
