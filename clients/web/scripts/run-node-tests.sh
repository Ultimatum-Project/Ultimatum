#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
cd "$web_directory"
node --test tests/*.test.cjs
