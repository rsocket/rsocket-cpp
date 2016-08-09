#!/usr/bin/env bash
set -xue

cd "$(dirname "$0")/.."
find src/ test/ tck-test/ -type f '(' -name '*.cpp' -o -name '*.h' ')' -exec clang-format -style=file -i {} \;

# EOF

