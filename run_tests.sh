#!/bin/bash

set -u

PASSED=0
FAILED=0
declare -a FAILED_TESTS=()

for file in tests/*.txt; do
    [[ -f "$file" ]] || continue
    echo "--------------------------------------------------"
    echo "Running: $file"

    EXPECTED=$(sed -n 's|^[[:space:]]*// EXPECTED:[[:space:]]*||p' "$file" | head -n 1)
    EXPECTED_DIAG=$(sed -n 's|^[[:space:]]*// EXPECTED_DIAG:[[:space:]]*||p' "$file" | head -n 1)
    EXPECTED_DIAG_LINE=$(sed -n 's|^[[:space:]]*// EXPECTED_DIAG_LINE:[[:space:]]*||p' "$file" | head -n 1)
    EXPECTED_WARNING=$(sed -n 's|^[[:space:]]*// EXPECTED_WARNING:[[:space:]]*||p' "$file" | head -n 1)
    EXPECTED_SYMBOL=$(sed -n 's|^[[:space:]]*// EXPECTED_SYMBOL:[[:space:]]*||p' "$file" | head -n 1)

    set +e
    OUTPUT=$(./compiler "$file" 2>&1)
    EXIT_CODE=$?
    set -e

    IS_PASS=1
    EXPECTED_ERRORS=$(echo "$EXPECTED" | sed -nE 's/^([0-9]+) errors?$/\1/p')
    if [[ "$EXPECTED" == "Parsing successful." ]]; then
        if [[ $EXIT_CODE -ne 0 ]] || ! grep -Fq 'Parsing successful.' <<< "$OUTPUT" || grep -Fq 'Compilation failed' <<< "$OUTPUT"; then
            IS_PASS=0
        fi
    elif [[ -n "$EXPECTED_ERRORS" ]]; then
        if [[ $EXIT_CODE -eq 0 ]] || ! grep -Fq "Compilation failed with $EXPECTED_ERRORS errors." <<< "$OUTPUT" || grep -Fq 'Parsing successful.' <<< "$OUTPUT"; then
            IS_PASS=0
        fi
    else
        echo "FAIL: malformed or missing EXPECTED comment"
        IS_PASS=0
    fi

    if [[ -n "$EXPECTED_DIAG" ]] && ! grep -Fqi -- "$EXPECTED_DIAG" <<< "$OUTPUT"; then
        echo "FAIL: expected diagnostic not found: $EXPECTED_DIAG"
        IS_PASS=0
    fi
    if [[ -n "$EXPECTED_DIAG_LINE" ]] && ! grep -F "Line $EXPECTED_DIAG_LINE: " <<< "$OUTPUT" | grep -Fq -- "$EXPECTED_DIAG"; then
        echo "FAIL: expected diagnostic on line $EXPECTED_DIAG_LINE: $EXPECTED_DIAG"
        IS_PASS=0
    fi
    if [[ -n "$EXPECTED_WARNING" ]] && ! grep -Fqi -- "$EXPECTED_WARNING" <<< "$OUTPUT"; then
        echo "FAIL: expected warning not found: $EXPECTED_WARNING"
        IS_PASS=0
    fi
    if [[ -n "$EXPECTED_SYMBOL" ]] && ! grep -Fq -- "$EXPECTED_SYMBOL" <<< "$OUTPUT"; then
        echo "FAIL: expected symbol-table entry not found: $EXPECTED_SYMBOL"
        IS_PASS=0
    fi

    if [[ $IS_PASS -eq 1 ]]; then
        echo "VERDICT: [PASS]"
        PASSED=$((PASSED+1))
    else
        echo "VERDICT: [FAIL]"
        FAILED=$((FAILED+1))
        FAILED_TESTS+=("$file")
    fi
    echo "$OUTPUT"
done

echo "=================================================="
echo "PASSED: $PASSED"
echo "FAILED: $FAILED"

if [[ $FAILED -gt 0 ]]; then
    printf 'Failed tests:\n'
    printf '  - %s\n' "${FAILED_TESTS[@]}"
    exit 1
fi
exit 0
