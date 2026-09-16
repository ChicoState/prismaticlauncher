#!/usr/bin/env bash
set -euo pipefail

cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage --output-on-failure

if find build/coverage -name '*.gcda' -print -quit | grep -q .; then
    mkdir -p coverage
    gcovr --root . --exclude 'build/' --fail-under-line 60 --xml-pretty --output coverage/coverage.xml
    gcovr --root . --exclude 'build/' --html-details coverage/coverage.html
else
    echo 'Coverage collection is deferred until product test targets are added.'
fi
