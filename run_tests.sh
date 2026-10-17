#!/usr/bin/env bash
set -uo pipefail
cd -- "$(dirname -- "$0")"

compiler_command=(./compiler)
if [ "$#" -eq 1 ] && [ "$1" = --rpn ]; then
    compiler_command+=(--rpn)
elif [ "$#" -ne 0 ]; then
    printf 'Usage: %s [--rpn]\n' "$0" >&2
    exit 2
fi

if ! ./build.sh; then
    printf 'Build failed; tests were not run.\n' >&2
    exit 1
fi

shopt -s nullglob
test_files=(tests/*.txt)
if [ "${#test_files[@]}" -eq 0 ]; then
    printf 'No test files found.\n' >&2
    exit 1
fi

for file in "${test_files[@]}"; do
    printf '\n========================================================\n'
    printf 'File: %s\n' "$file"
    printf '%s\n' '--------------------------------------------------------'
    cat -- "$file"
    printf '\n%s\n' '--------------------------------------------------------'
    printf 'Compiler output:\n'
    "${compiler_command[@]}" "$file" 2>&1
    printf 'Compiler exit status: %d\n' "$?"
done
