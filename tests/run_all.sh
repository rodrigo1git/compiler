#!/usr/bin/env bash
set -euo pipefail
project_directory=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd -- "$project_directory"
./build.sh
metadata_test=$(mktemp "${TMPDIR:-/tmp}/compiler-symbol-metadata.XXXXXX")
trap 'rm -f -- "$metadata_test"' EXIT
gcc -Wall -Wextra -Werror -std=gnu99 -Iinclude \
    tests/test_symbol_metadata.c src/symbol_table.c src/memory.c -o "$metadata_test"
"$metadata_test"
python3 tests/verify_function_docs.py
python3 tests/test_verify_tp4.py
python3 tests/verify_tp3.py
python3 tests/verify_tp3_course.py
python3 tests/verify_outputs.py
python3 tests/verify_tp4.py "$@"
