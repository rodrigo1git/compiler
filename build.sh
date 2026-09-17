#!/bin/bash

set -e

echo "Cleaning old build files..."
rm -f compilador compiler y.tab.c y.tab.h y.output

echo "Generating parser..."
bison -d -o y.tab.c src/grammar.y

echo "Compiling C source files..."
gcc -Wall -Wextra -std=gnu99 -Iinclude -I. \
    src/main.c \
    src/lexer.c \
    src/semantic_actions.c \
    src/transition_table.c \
    src/symbol_table.c \
    y.tab.c \
    -o compilador

echo "Build successful."

if [ -n "$1" ]; then
    echo "--------------------------------------------------------"
    echo "Running compiler with: $1"
    ./compilador "$1"
else
    echo "Usage: ./build.sh <input_file>"
fi
