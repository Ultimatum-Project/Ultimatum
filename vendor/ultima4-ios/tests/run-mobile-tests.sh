#!/bin/sh
set -eu
TEST_SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MOBILE_SOURCE_DIR="$TEST_SOURCE_DIR/../src"
MOBILE_TEST_BUILD=$(mktemp -d "${TMPDIR:-/tmp}/ultima4-mobile-tests.XXXXXX")
trap 'rm -rf "$MOBILE_TEST_BUILD"' EXIT HUP INT TERM
CC=${CC:-clang}
CXX=${CXX:-clang++}
"$CC" -std=c11 -I"$MOBILE_SOURCE_DIR" -I"$MOBILE_SOURCE_DIR/../deps/miniz" \
    "$TEST_SOURCE_DIR/vga_overlay_test.c" "$MOBILE_SOURCE_DIR/u4file.c" \
    "$MOBILE_SOURCE_DIR/../deps/miniz/miniz.c" -lz -o "$MOBILE_TEST_BUILD/vga"
mkdir "$MOBILE_TEST_BUILD/vga-zip" "$MOBILE_TEST_BUILD/vga-loose"
(cd "$MOBILE_TEST_BUILD/vga-zip" && ../vga)
(cd "$MOBILE_TEST_BUILD/vga-loose" && ../vga loose)
"$CC" -std=c11 -I"$MOBILE_SOURCE_DIR" -c "$MOBILE_SOURCE_DIR/savegame.c" -o "$MOBILE_TEST_BUILD/savegame.o"
"$CC" -std=c11 -I"$MOBILE_SOURCE_DIR" -c "$MOBILE_SOURCE_DIR/io.c" -o "$MOBILE_TEST_BUILD/io.o"
"$CC" -std=c11 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/error.c" "$MOBILE_SOURCE_DIR/experience_settings.c" \
    "$MOBILE_SOURCE_DIR/settings.c" "$TEST_SOURCE_DIR/experience_settings_test.c" \
    -o "$MOBILE_TEST_BUILD/experience"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/topicjournal.cpp" "$TEST_SOURCE_DIR/topicjournal_test.cpp" \
    -o "$MOBILE_TEST_BUILD/topics"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/topicjournal.cpp" "$MOBILE_SOURCE_DIR/journal_notebook.cpp" \
    "$TEST_SOURCE_DIR/journal_notebook_test.cpp" -o "$MOBILE_TEST_BUILD/notebook"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/mobile_rules_test.cpp" -o "$MOBILE_TEST_BUILD/rules"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/mobile_combat_test.cpp" -o "$MOBILE_TEST_BUILD/combat"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/mobile_map_pins.cpp" "$TEST_SOURCE_DIR/mobile_map_pins_test.cpp" \
    -o "$MOBILE_TEST_BUILD/map-pins"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/mobile_map_discoveries.cpp" "$TEST_SOURCE_DIR/mobile_map_discoveries_test.cpp" \
    -o "$MOBILE_TEST_BUILD/map-discoveries"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/mobile_dungeon_exploration.cpp" "$TEST_SOURCE_DIR/mobile_dungeon_exploration_test.cpp" \
    -o "$MOBILE_TEST_BUILD/dungeon-exploration"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/mobile_dungeon_sight.cpp" "$TEST_SOURCE_DIR/mobile_dungeon_sight_test.cpp" \
    -o "$MOBILE_TEST_BUILD/dungeon-sight"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/native_engine_session_test.cpp" -o "$MOBILE_TEST_BUILD/native-session"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/save_recovery_test.cpp" -o "$MOBILE_TEST_BUILD/recovery"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/save_snapshot_test.cpp" -o "$MOBILE_TEST_BUILD/snapshots"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/save_store_contract_test.cpp" -o "$MOBILE_TEST_BUILD/save-store-contract"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$TEST_SOURCE_DIR/save_slots_test.cpp" -o "$MOBILE_TEST_BUILD/slots"
"$CXX" -std=c++14 -Wall -Wextra -Werror -I"$MOBILE_SOURCE_DIR" \
    "$MOBILE_SOURCE_DIR/topicjournal.cpp" "$MOBILE_SOURCE_DIR/mobile_map_pins.cpp" \
    "$MOBILE_SOURCE_DIR/mobile_map_discoveries.cpp" "$MOBILE_SOURCE_DIR/mobile_dungeon_exploration.cpp" \
    "$TEST_SOURCE_DIR/adventure_snapshot_test.cpp" \
    "$MOBILE_TEST_BUILD/savegame.o" "$MOBILE_TEST_BUILD/io.o" -o "$MOBILE_TEST_BUILD/adventures"
for MOBILE_TEST_CASE in experience topics notebook rules combat map-pins map-discoveries dungeon-exploration dungeon-sight native-session recovery snapshots save-store-contract slots adventures; do
    "$MOBILE_TEST_BUILD/$MOBILE_TEST_CASE"
    printf '%s passed\n' "$MOBILE_TEST_CASE"
done
