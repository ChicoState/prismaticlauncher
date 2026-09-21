#!/usr/bin/env bash
set -euo pipefail

required_files=(
    CMakeLists.txt
    CMakePresets.json
    vcpkg.json
    vcpkg-configuration.json
    .clang-format
    .clang-tidy
    cppcheck.cfg
    .gitleaks.toml
    Dockerfile.ci
    .github/workflows/pr-checks.yml
    .github/workflows/release.yml
)

for file in "${required_files[@]}"; do
    test -f "$file" || { echo "Missing infrastructure file: $file" >&2; exit 1; }
done

python3 - <<'PY'
import json
for name in ("CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json"):
    with open(name, encoding="utf-8") as file:
        json.load(file)
PY

cmake --preset dev
ctest --preset dev --show-only=json-v1 >/dev/null

echo 'Infrastructure configuration is valid. Application targets and tests are not created yet.'
