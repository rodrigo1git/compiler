#!/usr/bin/env bash
set -euo pipefail
script_directory=$(cd -- "$(dirname -- "$0")" && pwd)
exec node "$script_directory/runtime/compile.mjs" "$@"
