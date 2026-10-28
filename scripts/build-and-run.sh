#!/usr/bin/env bash
# Configure, build, test, and run the developer app from any working directory.
set -euo pipefail

repository_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd -- "$repository_root"

cmake --preset dev
cmake --build --preset dev -j 2
ctest --preset dev --output-on-failure

exec "$repository_root/build/dev/bin/prismatic-launch" "$@"
