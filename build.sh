#!/bin/bash

set -e

if [ "$#" -gt 1 ]; then
    echo "Usage: $0 [input_file]" >&2
    exit 2
fi

echo "Cleaning old build files..."
rm -f compiler y.tab.c y.tab.h y.output

echo "Generating parser..."
bison -d -o y.tab.c src/grammar.y

echo "Compiling C source files..."
gcc -Wall -Wextra -Werror -std=gnu99 -Iinclude -I. \
    src/main.c \
    src/lexer.c \
    src/semantic_actions.c \
    src/transition_table.c \
    src/symbol_table.c \
    y.tab.c \
    -o compiler

echo "Build successful."

if [ "$#" -eq 1 ]; then
    echo "--------------------------------------------------------"
    echo "Running compiler with: $1"
    ./compiler "$1"
fi
