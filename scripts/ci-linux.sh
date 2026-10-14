#!/usr/bin/env bash
set -euo pipefail

cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage --output-on-failure

if find build/coverage -name '*.gcda' -print -quit | grep -q .; then
    mkdir -p coverage
    # The coverage floor applies to tested core logic, not presentation or test code.
    gcovr --root . --filter 'prismatic-launcher/src/qt/LauncherConfig.cpp' --fail-under-line 60 --xml-pretty --output coverage/coverage.xml
    gcovr --root . --filter 'prismatic-launcher/src/qt/LauncherConfig.cpp' --html-details coverage/coverage.html
else
    echo 'Coverage collection is deferred until product test targets are added.'
fi
