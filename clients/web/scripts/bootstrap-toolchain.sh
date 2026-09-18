#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
cache_directory="$web_directory/.cache"
emscripten_cache="$cache_directory/emscripten"
sdl_source="$cache_directory/ports/SDL2"
libxml_source="$cache_directory/deps/libxml2"
libxml_build="$cache_directory/build/libxml2"
prefix_directory="$cache_directory/prefix"

# shellcheck source=toolchain-env.sh
source "$script_directory/toolchain-env.sh"

python_command=${PYTHON:-python}
if ! command -v "$python_command" >/dev/null 2>&1; then
    printf 'Missing required command: %s\n' "$python_command" >&2
    exit 1
fi

if [[ ! -x "$tooling_root/Scripts/python.exe" && ! -x "$tooling_root/bin/python" ]]; then
    "$python_command" -m venv "$tooling_root"
fi
tooling_python="$tooling_root/bin/python"
tooling_bin="$tooling_root/bin"
if [[ -x "$tooling_root/Scripts/python.exe" ]]; then
    tooling_python="$tooling_root/Scripts/python.exe"
    tooling_bin="$tooling_root/Scripts"
fi
installed_cmake=$($tooling_python -c "import importlib.metadata as m; print(m.version('cmake'))" 2>/dev/null || true)
installed_ninja=$($tooling_python -c "import importlib.metadata as m; print(m.version('ninja'))" 2>/dev/null || true)
if [[ "$installed_cmake" != "$CMAKE_PYTHON_VERSION" || "$installed_ninja" != "$NINJA_PYTHON_VERSION" ]]; then
    "$tooling_python" -m pip install --disable-pip-version-check \
        "cmake==$CMAKE_PYTHON_VERSION" "ninja==$NINJA_PYTHON_VERSION"
fi
export PATH="$tooling_bin:$PATH"

if [[ ! -d "$emsdk_root/.git" ]]; then
    git clone https://github.com/emscripten-core/emsdk.git "$emsdk_root"
fi
if [[ "$(git -C "$emsdk_root" rev-parse HEAD)" != "$EMSDK_REPOSITORY_REVISION" ]]; then
    git -C "$emsdk_root" fetch origin "$EMSDK_REPOSITORY_REVISION"
    git -C "$emsdk_root" checkout --detach "$EMSDK_REPOSITORY_REVISION"
fi
if [[ ! -f "$emsdk_root/upstream/emscripten/emcc" ]]; then
    "$emsdk_root/emsdk" install "$EMSDK_VERSION"
fi
"$emsdk_root/emsdk" activate "$EMSDK_VERSION"
source "$emsdk_root/emsdk_env.sh" >/dev/null

for command_name in emcc emcmake cmake git ninja; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$command_name" >&2
        exit 1
    fi
done

mkdir -p "$cache_directory/ports" "$cache_directory/deps" "$cache_directory/build" "$prefix_directory"

if [[ ! -d "$sdl_source/.git" ]]; then
    git clone --depth 1 --branch "$SDL_GIT_TAG" https://github.com/libsdl-org/SDL.git "$sdl_source"
fi

if [[ ! -d "$libxml_source/.git" ]]; then
    git clone --depth 1 --branch "$LIBXML_GIT_TAG" https://github.com/GNOME/libxml2.git "$libxml_source"
fi

if [[ ! -f "$prefix_directory/lib/libxml2.a" ]]; then
    EM_CACHE="$emscripten_cache" emcmake cmake \
        -S "$libxml_source" \
        -B "$libxml_build" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix_directory" \
        -DBUILD_SHARED_LIBS=OFF \
        -DLIBXML2_WITH_PROGRAMS=OFF \
        -DLIBXML2_WITH_TESTS=OFF \
        -DLIBXML2_WITH_ICONV=OFF \
        -DLIBXML2_WITH_MODULES=OFF \
        -DLIBXML2_WITH_THREADS=OFF \
        -DLIBXML2_WITH_ZLIB=OFF
    EM_CACHE="$emscripten_cache" cmake --build "$libxml_build" --target install --parallel
fi

EM_CACHE="$emscripten_cache" \
EMCC_LOCAL_PORTS="sdl2=$sdl_source" \
emcc "$web_directory/toolchain/sdl-smoke.c" \
    -sUSE_SDL=2 \
    -sENVIRONMENT=web \
    -o "$cache_directory/emscripten-sdl-smoke.html"

printf 'WebAssembly toolchain ready: Emscripten %s, CMake %s, Ninja %s.\n' \
    "$EMSDK_VERSION" "$(cmake --version | head -n 1 | awk '{print $3}')" "$(ninja --version)"
