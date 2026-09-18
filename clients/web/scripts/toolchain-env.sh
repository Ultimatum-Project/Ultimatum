#!/usr/bin/env bash
# Source this file from a web build script after resolving script_directory.
toolchain_versions="$web_directory/toolchain/versions.env"
if [[ ! -f "$toolchain_versions" ]]; then
    printf 'Missing toolchain version contract: %s\n' "$toolchain_versions" >&2
    exit 1
fi
# shellcheck source=../toolchain/versions.env
source "$toolchain_versions"

tooling_root="$cache_directory/tooling"
if [[ -d "$tooling_root/Scripts" ]]; then
    export PATH="$tooling_root/Scripts:$PATH"
elif [[ -d "$tooling_root/bin" ]]; then
    export PATH="$tooling_root/bin:$PATH"
fi

emsdk_root="$cache_directory/emsdk-sdk"
export EMSDK_QUIET=1
if [[ -f "$emsdk_root/emsdk_env.sh" ]]; then
    # emsdk prints a banner by default; builds only need its environment.
    source "$emsdk_root/emsdk_env.sh" >/dev/null
fi
