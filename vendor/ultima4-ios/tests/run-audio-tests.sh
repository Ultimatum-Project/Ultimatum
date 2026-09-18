#!/bin/bash
# Host mixer/decoder tests with disposable copies; never modifies source audio.
set -euo pipefail
AUDIO_TEST_SRC="$(cd "$(dirname "$0")/.." && pwd)"
AUDIO_TEST_WORK="$(mktemp -d "${TMPDIR:-/tmp}/zu4-audio-tests.XXXXXX")"
trap 'rm -rf "$AUDIO_TEST_WORK"' EXIT HUP INT TERM
mkdir -p "$AUDIO_TEST_WORK/xu4/music" "$AUDIO_TEST_WORK/empty"
cp -R "$AUDIO_TEST_SRC/music/hurin" "$AUDIO_TEST_WORK/xu4/music/hurin"
clang -std=c11 $(sdl2-config --cflags) -I"$AUDIO_TEST_SRC/src" \
  "$AUDIO_TEST_SRC/tests/soundtrack_test.c" "$AUDIO_TEST_SRC/src/music.c" \
  "$AUDIO_TEST_SRC/src/cmixer.c" "$AUDIO_TEST_SRC/src/stb_vorbis.c" \
  $(sdl2-config --libs) -o "$AUDIO_TEST_WORK/test"
(cd "$AUDIO_TEST_WORK/xu4" && SDL_AUDIODRIVER=dummy ../test)
(cd "$AUDIO_TEST_WORK/empty" && SDL_AUDIODRIVER=dummy ZU4_EMPTY_AUDIO=1 ../test)
clang -std=c11 -DZU4_WEB $(sdl2-config --cflags) -I"$AUDIO_TEST_SRC/src" \
  "$AUDIO_TEST_SRC/tests/web_audio_contract_test.c" "$AUDIO_TEST_SRC/src/music.c" \
  "$AUDIO_TEST_SRC/src/cmixer.c" "$AUDIO_TEST_SRC/src/stb_vorbis.c" \
  $(sdl2-config --libs) -o "$AUDIO_TEST_WORK/web-contract"
(cd "$AUDIO_TEST_WORK/empty" && SDL_AUDIODRIVER=dummy ../web-contract)
