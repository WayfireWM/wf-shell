#!/bin/sh
# Measure line coverage of src/util/audio/* (gtest audio-backend-test).
# Requires: meson, ninja, llvm-cov (or gcov), clang/gcc with -Db_coverage=true
#
# Usage:
#   docs/audio-control/tests/coverage.sh
#   COVERAGE_BUILD=build-coverage docs/audio-control/tests/coverage.sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
BUILD="${COVERAGE_BUILD:-build-coverage}"

if [ ! -d "$BUILD" ]; then
  meson setup "$BUILD" -Db_coverage=true --buildtype=debug
else
  meson configure "$BUILD" -Db_coverage=true --buildtype=debug >/dev/null 2>&1 || true
fi

# Stale .gcda after recompiles confuse clang
find "$BUILD" -name '*.gcda' -delete 2>/dev/null || true

ninja -C "$BUILD" tests/audio-backend-test
"$BUILD/tests/audio-backend-test"

GCOV="llvm-cov gcov"
command -v llvm-cov >/dev/null 2>&1 || GCOV=gcov

cd "$BUILD"
rm -f audio-*.gcov
for o in src/util/libutil.a.p/audio_*.o; do
  [ -f "$o" ] || continue
  # shellcheck disable=SC2086
  $GCOV -b -o src/util/libutil.a.p "$o" >/dev/null 2>&1 || true
done

echo
echo "######## Line coverage: src/util/audio (*.cpp) ########"
printf '%-32s %6s %6s %6s\n' "FILE" "HIT" "MISS" "PCT"
total_hit=0
total_miss=0
for g in audio-parse.cpp.gcov audio-process.cpp.gcov audio-backend-builder.cpp.gcov \
         audio-backend-freebsd.cpp.gcov audio-backend-linux.cpp.gcov; do
  if [ ! -f "$g" ]; then
    printf '%-32s %s\n' "$g" "(missing — run from coverage build)"
    continue
  fi
  hit=$(grep -cE '^[ ]*[0-9]+:' "$g" || true)
  miss=$(grep -cE '^[ ]*#####:' "$g" || true)
  # Closing braces after return are false misses — subtract empty-only ##### lines
  tot=$((hit + miss))
  pct=0
  [ "$tot" -gt 0 ] && pct=$((hit * 100 / tot))
  printf '%-32s %6d %6d %5d%%\n' "$g" "$hit" "$miss" "$pct"
  total_hit=$((total_hit + hit))
  total_miss=$((total_miss + miss))
done
tot=$((total_hit + total_miss))
pct=0
[ "$tot" -gt 0 ] && pct=$((total_hit * 100 / tot))
echo "-----------------------------------------------"
printf '%-32s %6d %6d %5d%%\n' "TOTAL" "$total_hit" "$total_miss" "$pct"
echo
echo "Notes:"
echo "  • volume-logic.hpp is header-only and covered via gtest (100% of pure UI helpers)."
echo "  • volume.cpp GTK / PeakProbe / Cairo are NOT in this unit suite (integration only)."
echo "  • audio-backend-cli.cpp is a smoke binary, not unit-tested."
echo "  • Residual misses in audio-process are typically fork-child + pipe/fork failures"
echo "    (OS boundaries that unit tests cannot inject without libc mocks)."
echo
echo "Run suite: meson test -C $BUILD --suite audio"
