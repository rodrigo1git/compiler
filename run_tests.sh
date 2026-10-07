#!/bin/bash

echo "=================================================="
echo "               COMPILER TEST SUITE                "
echo "=================================================="
echo ""

PASSED=0
FAILED=0
declare -a FAILED_TESTS

cat << 'PYEOF' > parse_lines.py
import re
import sys

def extract_lines(text):
    text = text.lower()
    lines = []
    pattern = r'(?:linea|lineas|line|lines)([\s,yand&marcadaencatheonat]+[0-9]+)+'
    matches = re.finditer(pattern, text)
    for m in matches:
        s = m.group(0)
        for num in re.finditer(r'\b\d+\b', s):
            lines.append(int(num.group()))
    return sorted(list(set(lines)))

if __name__ == '__main__':
    text = sys.argv[1]
    lines = extract_lines(text)
    print(" ".join(map(str, lines)))
PYEOF

for file in tests/*.txt; do
    echo "--------------------------------------------------"
    echo "Running: $file"
    EXPECTED=$(grep -i "// EXPECTED" "$file" | sed 's/\/\/ EXPECTED:*\s*//i')
    echo "Test content and expected error:"
    cat "$file"
    echo ""
    echo "--- COMPILER OUTPUT ---"
    
    OUTPUT=$(./compiler "$file" 2>&1)
    EXIT_CODE=$?
    
    echo "$OUTPUT"
    echo ""
    
    # Validation logic
    EXPECTED_ERRORS=$(echo "$EXPECTED" | grep -oE '[0-9]+ (lexical|syntax|error|errors|errores|range)' | head -1 | awk '{print $1}')
    
    IS_PASS=0
    
    if echo "$EXPECTED" | grep -qi "successful"; then
        if echo "$OUTPUT" | grep -q "Parsing successful."; then
            IS_PASS=1
        fi
    elif [ -n "$EXPECTED_ERRORS" ]; then
        if echo "$OUTPUT" | grep -q "Compilation failed with $EXPECTED_ERRORS errors."; then
            IS_PASS=1
        fi
    else
        echo "FAIL: EXPECTED sin conteo interpretable"
        IS_PASS=0
    fi
    
    # Specific edge cases
    if echo "$EXPECTED" | grep -qi "Warning truncated"; then
        if echo "$OUTPUT" | grep -q "Parsing successful." && echo "$OUTPUT" | grep -qi "Warning"; then
            IS_PASS=1
        else
            IS_PASS=0
        fi
    fi

    # Line validation
    EXPECTED_LINES=$(python parse_lines.py "$EXPECTED")
    OUTPUT_LINES=$(echo "$OUTPUT" | grep '^Line ' | grep -oE '^Line [0-9]+' | grep -oE '[0-9]+' | sort -n | tr '\n' ' ' | sed 's/ *$//')

    if [ -n "$EXPECTED_LINES" ]; then
        if [ "$EXPECTED_LINES" != "$OUTPUT_LINES" ]; then
            echo "FAIL: Líneas reportadas no coinciden."
            echo "Esperadas: '$EXPECTED_LINES'"
            echo "Obtenidas: '$OUTPUT_LINES'"
            IS_PASS=0
        fi
    fi
    
    if [ $IS_PASS -eq 1 ]; then
        echo "VERDICT: [PASS]"
        PASSED=$((PASSED+1))
    else
        echo "VERDICT: [FAIL] - Output did not match expectations"
        FAILED=$((FAILED+1))
        FAILED_TESTS+=("$file")
    fi
done

echo "=================================================="
echo "                 TEST SUMMARY                     "
echo "=================================================="
echo "PASSED: $PASSED"
echo "FAILED: $FAILED"

rm parse_lines.py

if [ $FAILED -gt 0 ]; then
    echo "Failed tests:"
    for ft in "${FAILED_TESTS[@]}"; do
        echo "  - $ft"
    done
    exit 1
fi

exit 0
