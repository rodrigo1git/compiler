#!/usr/bin/env bash
set -euo pipefail
project_directory=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd -- "$project_directory"
./build.sh
python3 tests/verify_function_docs.py
python3 tests/verify_tp3.py
python3 tests/verify_tp3_course.py
python3 tests/verify_tp4.py "$@"
