#!/usr/bin/env bash

set -uo pipefail

if ! ./build.sh; then
    printf '%s\n' 'Compiler build failed.' >&2
    exit 1
fi

shopt -s nullglob
test_files=(tests/*.txt)

if ((${#test_files[@]} == 0)); then
    printf '%s\n' 'No test files found in tests/.'
    exit 1
fi

for file in "${test_files[@]}"; do
    printf '\n============================================================\n'
    printf 'Test file: %s\n' "$file"
    printf '%s\n' 'Test source:'
    cat -- "$file"

    printf '\nCommand: ./compiler %q\n' "$file"
    printf '%s\n' 'Compiler output:'
    ./compiler "$file" 2>&1 || :
done
