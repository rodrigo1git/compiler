#!/bin/bash

echo "=================================================="
echo "               COMPILER TEST SUITE                "
echo "=================================================="
echo ""

for file in tests/*.txt; do
    echo "--------------------------------------------------"
    echo "Running: $file"
    echo "Test content and expected error:"
    cat "$file"
    echo ""
    echo "--- COMPILER OUTPUT ---"
    ./compiler "$file"
    echo ""
done
