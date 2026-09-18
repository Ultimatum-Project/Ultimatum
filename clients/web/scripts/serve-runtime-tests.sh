#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"

if command -v python3 >/dev/null 2>&1; then
    python_command=python3
elif command -v python >/dev/null 2>&1; then
    python_command=python
else
    echo "Python 3 is required to serve the isolated runtime fixture." >&2
    exit 1
fi

exec "$python_command" "$web_directory/tests/serve-runtime.py" "$@"
