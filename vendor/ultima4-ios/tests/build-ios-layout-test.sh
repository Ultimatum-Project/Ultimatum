#!/bin/bash
# Standalone UIKit layout assertions; no engine adventures are loaded/changed.
set -euo pipefail
IOS_LAYOUT_SOURCE="$(cd "$(dirname "$0")" && pwd)"
IOS_LAYOUT_BUILD="$(mktemp -d /private/tmp/u4-native-layout.XXXXXX)"
IOS_LAYOUT_APP="$IOS_LAYOUT_BUILD/U4LayoutTest.app"
IOS_LAYOUT_SDK="$(xcrun --sdk iphonesimulator --show-sdk-path)"
mkdir -p "$IOS_LAYOUT_APP"
cp "$IOS_LAYOUT_SOURCE/ios_party_layout_test.plist" "$IOS_LAYOUT_APP/Info.plist"
xcrun --sdk iphonesimulator clang++ -target arm64-apple-ios13.0-simulator \
    -isysroot "$IOS_LAYOUT_SDK" -fobjc-arc -std=c++14 \
    -I"$IOS_LAYOUT_SOURCE/../src" \
    "$IOS_LAYOUT_SOURCE/ios_party_layout_test.mm" \
    -framework UIKit -framework Foundation -framework QuartzCore \
    -framework CoreGraphics -o "$IOS_LAYOUT_APP/U4LayoutTest"
printf 'Built native layout test: %s\n' "$IOS_LAYOUT_APP"
printf 'Install/launch on a simulator using org.ultimatumproject.tests.layout.\n'
printf 'Check the console for PASS and Documents/party-layout.png for a snapshot.\n'
