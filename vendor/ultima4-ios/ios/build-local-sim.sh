#!/bin/bash
# Build UltimatumU4 as a native iOS app for the iOS Simulator.
#
# Prereqs: Xcode + command-line tools, cmake (brew install cmake).
# Usage:
#   ios/build-local-sim.sh                  # clean app; first-run download
#   ios/build-local-sim.sh /path/to/ultima4 # optionally embed a local copy
set -euo pipefail

ZU4_SRC="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=load-local-env.sh
source "$ZU4_SRC/ios/load-local-env.sh"
zu4_load_local_env
WORK="${ZU4_BUILD_DIR:-/private/tmp/ultima4-device-build}"
SDL_VER="2.30.10"
ARCH="arm64"   # arm64 simulator (Apple Silicon); use x86_64 on Intel Macs.
BUNDLE_ID="${ZU4_IOS_BUNDLE_ID:-org.example.ultimatum.u4}"
U4_DATA="${1:-}"
zu4_prepare_account_assets "${ULTIMATUM_CLOUD_AUDIENCE:-test}"

mkdir -p "$WORK"; cd "$WORK"

data_ok() { { [ -f "$1/AVATAR.EXE" ] || [ -f "$1/avatar.exe" ]; } && { [ -f "$1/TITLE.EXE" ] || [ -f "$1/title.exe" ]; }; }
if [ -n "$U4_DATA" ] && ! data_ok "$U4_DATA"; then
  echo "ERROR: Ultima IV data in '$U4_DATA' is missing/incomplete (need AVATAR.EXE + TITLE.EXE)." >&2
  exit 1
fi
U4_UPGRADE="$("$ZU4_SRC/ios/prepare-vga-upgrade.sh" "$WORK")"

# 1. SDL2 static for the iOS Simulator (built once).
if [ ! -f "$WORK/sdl2-sim/Release-iphonesimulator/libSDL2.a" ]; then
  [ -d "SDL2-${SDL_VER}" ] || {
    curl -L -o SDL2.tar.gz \
      "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER}/SDL2-${SDL_VER}.tar.gz"
    tar xzf SDL2.tar.gz
  }
  rm -rf sdl2-sim && mkdir sdl2-sim && cd sdl2-sim
  cmake "../SDL2-${SDL_VER}" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_TEST=OFF
  xcodebuild -project SDL2.xcodeproj -target SDL2-static -configuration Release \
    -sdk iphonesimulator -arch "${ARCH}"
  xcodebuild -project SDL2.xcodeproj -target SDL2main -configuration Release \
    -sdk iphonesimulator -arch "${ARCH}"
  cd "$WORK"
fi
SDL_SRC="$WORK/SDL2-${SDL_VER}"
SDL_LIBDIR="$WORK/sdl2-sim/Release-iphonesimulator"

# 2. Configure + build the app.
rm -rf zu4-sim && mkdir zu4-sim && cd zu4-sim
cmake "$ZU4_SRC" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES="${ARCH}" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DSDL2_INCLUDE_DIR="$SDL_SRC/include" \
  -DSDL2_LIBRARY="$SDL_LIBDIR/libSDL2.a" \
  -DSDL2MAIN_LIBRARY="$SDL_LIBDIR/libSDL2main.a" \
  -DZU4_U4_GAMEDIR="$U4_DATA" -DZU4_U4_UPGRADE="$U4_UPGRADE" -DZU4_IOS_BUNDLE_ID="$BUNDLE_ID" \
  -DZU4_ACCOUNT_UI="$ZU4_ACCOUNT_UI" -DZU4_CLOUD_SESSION_ACCOUNT="$ZU4_CLOUD_SESSION_ACCOUNT"
xcodebuild -project zu4.xcodeproj -target zu4 -configuration Release \
  -sdk iphonesimulator -arch "${ARCH}" CODE_SIGNING_ALLOWED=NO

APP="$WORK/zu4-sim/Release-iphonesimulator/UltimatumU4.app"
echo; echo "Built: $APP"; echo "Run with:"
echo "  xcrun simctl boot 'iPhone 15' 2>/dev/null; open -a Simulator"
echo "  xcrun simctl install booted '$APP'"
echo "  xcrun simctl launch booted $BUNDLE_ID"
echo "  (rotate the Simulator to landscape: Device > Rotate, or Cmd+Left)"
