#!/usr/bin/env bash
set -uo pipefail
cd -- "$(dirname -- "$0")"

compiler_command=(./compiler)
show=false
for option in "$@"; do
    case "$option" in
        --show) show=true ;;
        --rpn|--analyze|--trace) compiler_command+=("$option") ;;
        *) printf 'Usage: %s [--show [--rpn|--analyze] [--trace]]\n' "$0" >&2; exit 2 ;;
    esac
done

if ! ./build.sh; then
    printf 'Build failed; tests were not run.\n' >&2
    exit 1
fi

if [ "$show" = false ]; then
    status=0
    python3 tests/verify_tp3.py || status=1
    python3 tests/verify_tp3_course.py || status=1
    exit "$status"
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
