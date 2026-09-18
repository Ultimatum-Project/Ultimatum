#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
source_archive=${1:-}
target_archive="$web_directory/.cache/u4upgrad.zip"

if [[ -z "$source_archive" ]]; then
    printf 'Usage: %s /path/to/u4upgrad.zip\n' "$0" >&2
    exit 2
fi

if [[ ! -f "$source_archive" ]]; then
    printf 'VGA upgrade archive does not exist: %s\n' "$source_archive" >&2
    exit 1
fi

if ! unzip -Z1 "$source_archive" | tr '[:upper:]' '[:lower:]' | grep -qE '(^|/)u4vga\.pal$'; then
    printf 'That archive does not contain u4vga.pal and is not a usable xu4 VGA overlay.\n' >&2
    exit 1
fi

mkdir -p "$(dirname "$target_archive")"
cp "$source_archive" "$target_archive"

printf 'Imported the optional VGA overlay into %s\n' "$target_archive"
