#!/bin/bash
# Download and stage only the graphics portion of the Ultima IV VGA Upgrade.
set -euo pipefail

WORK="${1:?Usage: prepare-vga-upgrade.sh <build-cache-directory>}"
SOURCE="$WORK/u4upgrad-1.3.zip"
OUTPUT="$WORK/u4upgrad-graphics.zip"
EXPECTED_SHA256="400ac37311f3be74c1b2d7836561b2ead2b146f5162586865b0f4881225cca58"
URL="https://www.moongates.com/u4/upgrade/files/u4upgrad.zip"

mkdir -p "$WORK"
if [ ! -f "$SOURCE" ] || [ "$(shasum -a 256 "$SOURCE" | awk '{print $1}')" != "$EXPECTED_SHA256" ]; then
  echo "Downloading the Ultima IV VGA Upgrade graphics..." >&2
  curl -L --fail -o "$SOURCE.download" "$URL"
  ACTUAL_SHA256="$(shasum -a 256 "$SOURCE.download" | awk '{print $1}')"
  if [ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]; then
    rm -f "$SOURCE.download"
    echo "ERROR: VGA Upgrade checksum mismatch." >&2
    exit 1
  fi
  mv "$SOURCE.download" "$SOURCE"
fi

# Always rebuild from verified inputs: an old ZIP can retain unexpected members
# when zip updates it in place. Neither MIDPAK nor executables belong here.
  STAGE="$(mktemp -d "${TMPDIR:-/tmp}/zu4-vga.XXXXXX")"
  trap 'rm -rf "$STAGE"' EXIT HUP INT TERM
  unzip -oq "$SOURCE" -d "$STAGE" \
    shapes.vga charset.vga u4vga.pal start.old key7.old \
    honesty.old compassn.old valor.old justice.old sacrific.old honor.old \
    spirit.old humility.old truth.old love.old courage.old stoncrcl.old \
    rune_1.old rune_2.old rune_3.old rune_4.old rune_5.old rune_6.ega \
    rune_7.ega rune_8.ega Readme.txt
  if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN=python3
  elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN=python
  else
    echo "ERROR: Python 3 is required to create the deterministic graphics archive." >&2
    exit 1
  fi
  GENERATED="$OUTPUT.new"
  "$PYTHON_BIN" - "$STAGE" "$GENERATED" <<'PY'
import pathlib
import sys
import zipfile

stage = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
    for source in sorted(stage.iterdir(), key=lambda item: item.name.lower()):
        info = zipfile.ZipInfo(source.name, date_time=(2000, 1, 1, 0, 0, 0))
        info.create_system = 3
        info.external_attr = 0o100644 << 16
        archive.writestr(info, source.read_bytes())
PY
  if [ ! -f "$OUTPUT" ] || ! cmp -s "$GENERATED" "$OUTPUT"; then
    mv "$GENERATED" "$OUTPUT"
  else
    rm "$GENERATED"
  fi
  CREDITS="$WORK/ultima-iv-vga-readme.txt"
  if [ ! -f "$CREDITS" ] || ! cmp -s "$STAGE/Readme.txt" "$CREDITS"; then
    cp "$STAGE/Readme.txt" "$CREDITS"
  fi

printf '%s\n' "$OUTPUT"
