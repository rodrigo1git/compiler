#!/bin/bash

set -e

cd -- "$(dirname -- "$0")"

# Prefer a modern Homebrew Bison on macOS; allow an explicit override.
if [ -z "${BISON:-}" ]; then
    BISON=bison
    for candidate in /opt/homebrew/opt/bison/bin/bison /usr/local/opt/bison/bin/bison; do
        if [ -x "$candidate" ]; then
            BISON="$candidate"
            break
        fi
    done
fi
if ! "$BISON" --version >/dev/null 2>&1; then
    echo "Bison is required (version 3.6 or newer)." >&2
    exit 1
fi
version=$("$BISON" --version | head -n 1 | awk '{print $NF}')
if ! awk -v version="$version" 'BEGIN { split(version, v, "."); exit !(v[1] > 3 || (v[1] == 3 && v[2] >= 6)) }'; then
    echo "Bison 3.6 or newer is required; found $version. On macOS: brew install bison" >&2
    exit 1
fi

if [ "$#" -gt 1 ]; then
    echo "Usage: $0 [input_file]" >&2
    exit 2
fi

echo "Cleaning old build files..."
rm -f compiler y.tab.c y.tab.h y.output

echo "Generating parser..."
"$BISON" -Wall -Werror=conflicts-sr -Werror=conflicts-rr -d -o y.tab.c src/grammar.y

echo "Compiling C source files..."
gcc -Wall -Wextra -Werror -std=gnu99 -Iinclude -I. \
    src/main.c \
    src/diagnostics.c \
    src/lexer.c \
    src/semantic_actions.c \
    src/transition_table.c \
    src/symbol_table.c \
    src/scope.c \
    src/rpn.c \
    src/codegen.c \
    y.tab.c \
    -o compiler

echo "Build successful."

if [ "$#" -eq 1 ]; then
    echo "--------------------------------------------------------"
    echo "Running compiler with: $1"
    ./compiler "$1"
fi
