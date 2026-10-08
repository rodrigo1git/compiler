#!/usr/bin/env bash

set -uo pipefail

if ! ./build.sh; then
    echo "FAIL: compiler build failed"
    exit 1
fi

passed=0
failed=0
declare -a failed_tests=()
shopt -s nullglob
test_files=(tests/*.txt)

if ((${#test_files[@]} == 0)); then
    echo "FAIL: no tests found in tests/"
    exit 1
fi

for file in "${test_files[@]}"; do
    expected=""
    declare -a expected_diags=() expected_warnings=() expected_symbols=() expected_line_diags=() expected_syntax=()

    while IFS= read -r line || [[ -n "$line" ]]; do
        case "$line" in
            "// EXPECTED: "*) [[ -z "$expected" ]] && expected=${line#"// EXPECTED: "} ;;
            "// EXPECTED_DIAG: "*) expected_diags+=("${line#"// EXPECTED_DIAG: "}") ;;
            "// EXPECTED_WARNING: "*) expected_warnings+=("${line#"// EXPECTED_WARNING: "}") ;;
            "// EXPECTED_SYMBOL: "*) expected_symbols+=("${line#"// EXPECTED_SYMBOL: "}") ;;
            "// EXPECTED_DIAG_LINE: "*) expected_line_diags+=("${line#"// EXPECTED_DIAG_LINE: "}") ;;
            "// EXPECTED_SYNTAX: "*) expected_syntax+=("${line#"// EXPECTED_SYNTAX: "}") ;;
        esac
    done < "$file"

    output=$(timeout 10 ./compiler "$file" 2>&1)
    exit_code=$?
    is_pass=1
    actual_errors=$(grep -Ec '^Line [0-9]+: (Lexical|Syntax) error:' <<< "$output" || true)
    expected_errors=$(sed -nE 's/^([0-9]+) errors?$/\1/p' <<< "$expected")

    if [[ "$expected" == "Parsing successful." ]]; then
        if ((exit_code != 0)) || ! grep -Fq 'Parsing successful.' <<< "$output" || grep -Fq 'Compilation failed' <<< "$output" || ((actual_errors != 0)); then
            is_pass=0
        fi
    elif [[ -n "$expected_errors" ]]; then
        if ((exit_code == 0)) || ! grep -Fq "Compilation failed with $expected_errors errors." <<< "$output" || grep -Fq 'Parsing successful.' <<< "$output" || ((actual_errors != expected_errors)); then
            is_pass=0
        fi
    else
        echo "FAIL: $file has a missing or invalid EXPECTED assertion"
        is_pass=0
    fi

    for diagnostic in "${expected_diags[@]}"; do
        if ! grep -Fqi -- "$diagnostic" <<< "$output"; then
            echo "FAIL: $file missing diagnostic: $diagnostic"
            is_pass=0
        fi
    done
    for warning in "${expected_warnings[@]}"; do
        if ! grep -Fqi -- "$warning" <<< "$output"; then
            echo "FAIL: $file missing warning: $warning"
            is_pass=0
        fi
    done
    for symbol in "${expected_symbols[@]}"; do
        if ! grep -Fq -- "$symbol" <<< "$output"; then
            echo "FAIL: $file missing symbol-table entry: $symbol"
            is_pass=0
        fi
    done
    for syntax_event in "${expected_syntax[@]}"; do
        if ! grep -Fq -- "$syntax_event" <<< "$output"; then
            echo "FAIL: $file missing parsed syntax event: $syntax_event"
            is_pass=0
        fi
    done
    for line_diagnostic in "${expected_line_diags[@]}"; do
        if [[ "$line_diagnostic" == *:* ]]; then
            line_number=${line_diagnostic%%:*}
            diagnostic=${line_diagnostic#*: }
        else
            line_number=$line_diagnostic
            diagnostic=""
        fi
        if ! grep -F "Line $line_number: " <<< "$output" | grep -Fq -- "$diagnostic"; then
            echo "FAIL: $file missing diagnostic on line $line_number: $diagnostic"
            is_pass=0
        fi
    done

    if ((is_pass)); then
        echo "PASS: $file"
        ((passed += 1))
    else
        echo "FAIL: $file (exit $exit_code, expected '$expected', saw $actual_errors diagnostic lines)"
        printf '%s\n' "$output"
        failed_tests+=("$file")
        ((failed += 1))
    fi
done

printf 'PASSED: %d\nFAILED: %d\n' "$passed" "$failed"
if ((failed)); then
    printf 'Failed tests:\n'
    printf '  - %s\n' "${failed_tests[@]}"
    exit 1
fi
