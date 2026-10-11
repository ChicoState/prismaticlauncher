#!/usr/bin/env bash
set -euo pipefail

mapfile -d '' sources < <(find src prismatic-launcher/ui tests -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.h' -o -name '*.hpp' \) -print0 2>/dev/null || true)

if ((${#sources[@]} == 0)); then
    echo 'No C++ source files exist yet; clang-tidy and Cppcheck are deferred.'
    exit 0
fi

build_directory="${PRISMATIC_BUILD_DIR:-build/dev}"
if [[ ! -f "$build_directory/compile_commands.json" && -f build/coverage/compile_commands.json ]]; then
    build_directory=build/coverage
fi
if [[ ! -f "$build_directory/compile_commands.json" && -z "${PRISMATIC_BUILD_DIR:-}" ]]; then
    cmake --preset dev
    cmake --build --preset dev
fi
test -f "$build_directory/compile_commands.json" || {
    echo 'Configure and build with CMAKE_EXPORT_COMPILE_COMMANDS=ON before analysis.' >&2
    exit 1
}
clang-tidy -p "$build_directory" --config-file=.clang-tidy "${sources[@]}"
cppcheck --project="$build_directory/compile_commands.json" --file-filter='prismatic-launcher/ui/*' --file-filter='src/*' --file-filter='tests/*' $(< cppcheck.cfg)
