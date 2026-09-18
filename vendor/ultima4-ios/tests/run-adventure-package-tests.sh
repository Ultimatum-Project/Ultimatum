#!/bin/sh
set -eu
PACKAGE_TEST_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_SOURCE_DIR="$PACKAGE_TEST_DIR/../src"
PACKAGE_BUILD=$(mktemp -d "${TMPDIR:-/tmp}/ultima4-package-tests.XXXXXX")
trap 'rm -rf "$PACKAGE_BUILD"' EXIT HUP INT TERM
clang -std=c11 -I"$PACKAGE_SOURCE_DIR" -c "$PACKAGE_SOURCE_DIR/savegame.c" -o "$PACKAGE_BUILD/savegame.o"
clang -std=c11 -I"$PACKAGE_SOURCE_DIR" -c "$PACKAGE_SOURCE_DIR/io.c" -o "$PACKAGE_BUILD/io.o"
clang++ -std=c++14 -fobjc-arc -Wall -Wextra -Werror -I"$PACKAGE_SOURCE_DIR" -I"$PACKAGE_TEST_DIR/../ios" \
    "$PACKAGE_TEST_DIR/../ios/adventure_package.mm" "$PACKAGE_TEST_DIR/adventure_package_test.mm" \
    "$PACKAGE_SOURCE_DIR/topicjournal.cpp" "$PACKAGE_SOURCE_DIR/journal_notebook.cpp" \
    "$PACKAGE_SOURCE_DIR/mobile_map_pins.cpp" "$PACKAGE_SOURCE_DIR/mobile_map_discoveries.cpp" \
    "$PACKAGE_SOURCE_DIR/mobile_dungeon_exploration.cpp" "$PACKAGE_BUILD/savegame.o" "$PACKAGE_BUILD/io.o" \
    -framework Foundation -lz -o "$PACKAGE_BUILD/package-test"
"$PACKAGE_BUILD/package-test"
node "$PACKAGE_TEST_DIR/adventure-package-roundtrip.cjs" "$PACKAGE_BUILD/package-test"
if [ -n "${ZU4_PACKAGE_FIXTURE_DIR:-}" ]; then
    "$PACKAGE_BUILD/package-test" --world-fixture "$ZU4_PACKAGE_FIXTURE_DIR/world.u4save"
fi
if [ -n "${ZU4_PACKAGE_BROWSER_BACKUP:-}" ]; then
    "$PACKAGE_BUILD/package-test" --roundtrip "$ZU4_PACKAGE_BROWSER_BACKUP" "$PACKAGE_BUILD/browser-return.u4save"
    echo "Real browser export → native validated slot installation: passed"
fi
