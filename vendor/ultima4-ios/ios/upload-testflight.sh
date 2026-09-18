#!/bin/bash
# Archive Ultimatum U4 and upload it to App Store Connect/TestFlight.
# The resulting app deliberately excludes the original Ultima IV game data.
#
# Usage: ios/upload-testflight.sh [AppleTeamID]
# Optional environment:
#   ZU4_IOS_TEAM           Apple Team ID (or pass it as the first argument)
#   ZU4_IOS_BUNDLE_ID      App Store bundle identifier
#   ZU4_IOS_VERSION        Marketing version (default 1.0)
#   ZU4_IOS_BUILD_NUMBER   TestFlight build number (default 1)
#   ZU4_BUILD_DIR          Dependency/build root
set -euo pipefail

ZU4_SRC="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=load-local-env.sh
source "$ZU4_SRC/ios/load-local-env.sh"
zu4_load_local_env
TEAM="${1:-${ZU4_IOS_TEAM:-}}"
WORK="${ZU4_BUILD_DIR:-/private/tmp/ultima4-device-build}"
SDL_VER="2.30.10"
BUNDLE_ID="${ZU4_IOS_BUNDLE_ID:-org.example.ultimatum.u4}"
VERSION="${ZU4_IOS_VERSION:-1.0}"
BUILD_NUMBER="${ZU4_IOS_BUILD_NUMBER:-1}"
SDL_SRC="$WORK/SDL2-${SDL_VER}"
SDL_LIBDIR="$WORK/sdl2-device/Release-iphoneos"
PROJECT_DIR="$WORK/ultimatum-u4-appstore"
ARCHIVE="$WORK/UltimatumU4.xcarchive"
EXPORT_DIR="$WORK/testflight-export"

if ! [[ "$TEAM" =~ ^[A-Za-z0-9]{10}$ ]]; then
  echo "ERROR: set ZU4_IOS_TEAM in .env.local or pass a 10-character Apple Team ID." >&2
  exit 1
fi
zu4_prepare_account_assets "${ULTIMATUM_CLOUD_AUDIENCE:-public}" required

if [ ! -f "$SDL_LIBDIR/libSDL2.a" ] || [ ! -f "$SDL_LIBDIR/libSDL2main.a" ]; then
  echo "ERROR: Device SDL libraries are missing under '$SDL_LIBDIR'." >&2
  echo "Run ios/build-local-device.sh $TEAM once, then retry." >&2
  exit 1
fi
U4_UPGRADE="$("$ZU4_SRC/ios/prepare-vga-upgrade.sh" "$WORK")"

rm -rf "$PROJECT_DIR" "$ARCHIVE" "$EXPORT_DIR"
mkdir -p "$PROJECT_DIR" "$EXPORT_DIR"
cmake -S "$ZU4_SRC" -B "$PROJECT_DIR" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DSDL2_INCLUDE_DIR="$SDL_SRC/include" \
  -DSDL2_LIBRARY="$SDL_LIBDIR/libSDL2.a" \
  -DSDL2MAIN_LIBRARY="$SDL_LIBDIR/libSDL2main.a" \
  -DZU4_U4_GAMEDIR= \
  -DZU4_U4_UPGRADE="$U4_UPGRADE" \
  -DZU4_IOS_TEAM="$TEAM" \
  -DZU4_IOS_BUNDLE_ID="$BUNDLE_ID" \
  -DZU4_IOS_VERSION="$VERSION" \
  -DZU4_IOS_BUILD_NUMBER="$BUILD_NUMBER" \
  -DZU4_ACCOUNT_UI="$ZU4_ACCOUNT_UI" \
  -DZU4_CLOUD_SESSION_ACCOUNT="$ZU4_CLOUD_SESSION_ACCOUNT"

xcodebuild -project "$PROJECT_DIR/zu4.xcodeproj" -scheme zu4 \
  -configuration Release -destination "generic/platform=iOS" \
  -archivePath "$ARCHIVE" -allowProvisioningUpdates \
  DEVELOPMENT_TEAM="$TEAM" archive

xcodebuild -exportArchive -archivePath "$ARCHIVE" \
  -exportPath "$EXPORT_DIR" \
  -exportOptionsPlist "$PROJECT_DIR/ExportOptions-TestFlight.plist" \
  -allowProvisioningUpdates

echo "Uploaded Ultimatum U4 $VERSION ($BUILD_NUMBER) to App Store Connect."
