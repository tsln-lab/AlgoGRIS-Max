#!/usr/bin/env bash
# Builds tests/engine_smoke.cpp against an existing AlgoGRIS build (Linux/macOS) and runs it.
# The data it needs (setups/, support/algogris-data/) comes from configuring the package once;
# if it's missing, this copies it from AlgoGRIS directly.
# Usage: tests/run_engine_smoke.sh [path/to/AlgoGRIS]
set -euo pipefail

PKG="$(cd "$(dirname "$0")/.." && pwd)"
ALGOGRIS="$(cd "${1:-$PKG/../AlgoGRIS}" && pwd)"
TARGET="$ALGOGRIS/build/CMakeFiles/test_core.dir"
OUT="$PKG/build/engine_smoke"
mkdir -p "$OUT" "$PKG/setups" "$PKG/support/algogris-data/tests/util"

[ -d "$PKG/support/algogris-data/hrtf_compact" ] || cp -r "$ALGOGRIS/hrtf_compact" "$PKG/support/algogris-data/"
copy_missing() { [ -f "$2/$(basename "$1")" ] || cp "$1" "$2/"; }
copy_missing "$ALGOGRIS/tests/util/BINAURAL_SPEAKER_SETUP.xml" "$PKG/support/algogris-data/tests/util"
copy_missing "$ALGOGRIS/tests/temp/Dome_default_speaker_setup.xml" "$PKG/setups"
copy_missing "$ALGOGRIS/tests/util/Cube_default_speaker_setup.xml" "$PKG/setups"

# Same flags as AlgoGRIS's test_core, minus ALGOGRIS_UNIT_TESTS (as in the Max build).
DEFINES=$(sed -n 's/^CXX_DEFINES = //p' "$TARGET/flags.make" | sed 's/-DALGOGRIS_UNIT_TESTS//')
INCLUDES=$(sed -n 's/^CXX_INCLUDES = //p' "$TARGET/flags.make")
for f in "$PKG/source/projects/algogris_tilde/algogris_engine.cpp" "$PKG/tests/engine_smoke.cpp"; do
    echo "== compiling $(basename "$f")"
    eval c++ -std=gnu++20 -O2 -DNDEBUG $DEFINES $INCLUDES -c "\"$f\"" -o "\"$OUT/$(basename "$f" .cpp).o\""
done

LINK=$(sed -e "s| CMakeFiles/test_core.dir/tests/unit/test_core.cpp.o | $OUT/algogris_engine.o $OUT/engine_smoke.o |" \
           -e "s| -o test_core | -o $OUT/engine_smoke |" \
           -e "s|--dependency-file=[^ ]*|--dependency-file=$OUT/link.d|" \
           -e "s| [^ ]*libMainTest.a | |" -e "s| [^ ]*libCatch2Main[^ ]*.a | |" \
           "$TARGET/link.txt")
echo "== linking"
(cd "$ALGOGRIS/build" && eval "$LINK")

echo "== running"
"$OUT/engine_smoke" "$PKG"
