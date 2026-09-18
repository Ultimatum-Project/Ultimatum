#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
source_directory=${1:-}
target_directory="$web_directory/.cache/game-data"

if [[ -z "$source_directory" ]]; then
    printf 'Usage: %s /path/to/ultima4-data\n' "$0" >&2
    exit 2
fi

if [[ ! -d "$source_directory" ]]; then
    printf 'Game-data directory does not exist: %s\n' "$source_directory" >&2
    exit 1
fi

data_file_exists() {
    local lower
    lower=$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')
    [[ -f "$source_directory/$1" || -f "$source_directory/$lower" ]]
}

if ! data_file_exists AVATAR.EXE || ! data_file_exists TITLE.EXE; then
    printf 'Incomplete Ultima IV data: AVATAR.EXE and TITLE.EXE are required.\n' >&2
    exit 1
fi

mkdir -p "$target_directory"
cp -R "$source_directory"/. "$target_directory"/

printf 'Imported local Ultima IV data into %s\n' "$target_directory"
