#!/usr/bin/env bash
set -euo pipefail
script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
cache_directory="$web_directory/.cache"
bundle_music=${ULTIMATUM_BUNDLE_MUSIC:-OFF}

# shellcheck source=toolchain-env.sh
source "$script_directory/toolchain-env.sh"
if ! command -v emcmake >/dev/null 2>&1; then
    "$script_directory/bootstrap-toolchain.sh"
    source "$script_directory/toolchain-env.sh"
fi

EM_CACHE="$cache_directory/emscripten" \
EMCC_LOCAL_PORTS="sdl2=$cache_directory/ports/SDL2" \
emcmake cmake -S "$web_directory" -B "$cache_directory/build/runtime" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DULTIMATUM_WEB_RUNTIME_TESTS=ON \
    -DULTIMATUM_WEB_DEBUG_TOOLS=ON \
    -DULTIMATUM_WEB_DIAGNOSTICS=ON \
    -DULTIMATUM_U4_UPGRADE="$cache_directory/assets/u4upgrad-graphics.zip" \
    -DULTIMATUM_BUNDLE_MUSIC="$bundle_music" \
    -DULTIMATUM_WEB_OUTPUT_DIRECTORY="$cache_directory/runtime/engine"

EM_CACHE="$cache_directory/emscripten" \
EMCC_LOCAL_PORTS="sdl2=$cache_directory/ports/SDL2" \
cmake --build "$cache_directory/build/runtime" --parallel "${ULTIMATUM_WEB_BUILD_JOBS:-2}"

printf 'Isolated test engine built. Run npm run test:runtime:serve.\n'
