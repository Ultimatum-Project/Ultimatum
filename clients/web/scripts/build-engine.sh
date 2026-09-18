#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
cache_directory="$web_directory/.cache"
emscripten_cache="$cache_directory/emscripten"
sdl_source="$cache_directory/ports/SDL2"
build_directory=${ULTIMATUM_WEB_BUILD_DIRECTORY:-$cache_directory/build/engine}
output_directory=${ULTIMATUM_WEB_OUTPUT_DIRECTORY:-$web_directory/dist/engine}
bundle_game_data=${ULTIMATUM_BUNDLE_U4_DATA:-ON}
debug_tools=${ULTIMATUM_WEB_DEBUG_TOOLS:-$bundle_game_data}
bundle_music=${ULTIMATUM_BUNDLE_MUSIC:-OFF}
build_jobs=${ULTIMATUM_WEB_BUILD_JOBS:-2}

# shellcheck source=toolchain-env.sh
source "$script_directory/toolchain-env.sh"
if [[ ! -f "$cache_directory/prefix/lib/libxml2.a" || ! -d "$sdl_source/.git" ]] || ! command -v emcmake >/dev/null 2>&1; then
    "$script_directory/bootstrap-toolchain.sh"
    source "$script_directory/toolchain-env.sh"
fi

vga_upgrade=$(bash "$web_directory/../../vendor/ultima4-ios/ios/prepare-vga-upgrade.sh" "$cache_directory/assets")
EM_CACHE="$emscripten_cache" \
EMCC_LOCAL_PORTS="sdl2=$sdl_source" \
emcmake cmake \
    -S "$web_directory" \
    -B "$build_directory" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DULTIMATUM_BUNDLE_U4_DATA="$bundle_game_data" \
    -DULTIMATUM_WEB_DEBUG_TOOLS="$debug_tools" \
    -DULTIMATUM_U4_UPGRADE="$vga_upgrade" \
    -DULTIMATUM_BUNDLE_MUSIC="$bundle_music" \
    -DULTIMATUM_WEB_OUTPUT_DIRECTORY="$output_directory" \
    -DULTIMATUM_WEB_RUNTIME_TESTS=OFF \
    -DULTIMATUM_WEB_DIAGNOSTICS=OFF

EM_CACHE="$emscripten_cache" \
EMCC_LOCAL_PORTS="sdl2=$sdl_source" \
cmake --build "$build_directory" --parallel "$build_jobs"

printf 'Engine artifacts written to %s\n' "$output_directory"
