#!/bin/bash
#
# Compile and run 12-installed-header-F3-checklist.cxx against the REAL installed
# header (larcoreobj/LoggingUtil/Logging.h) and the compiled spdlog library.
#
# This is the PROCEDURE.md F3 gate: "foundation test green". Run it before
# starting any migration, and after any edit to Logging.h / Logging.cxx.
#
# Unlike build-and-run.sh (which compiles header-only and works with the system
# compiler), this links libspdlog.so and therefore needs the same gcc the library
# was built with: the system g++ 11.5 lacks GLIBCXX_3.4.30 and fails at link time.
#
# Usage:  ./verify-installed-header.sh
# Env:    SPDLOG_DIR, GCC_DIR, SRCS_DIR to override the cvmfs/source locations.

set -u

SPDLOG_DIR="${SPDLOG_DIR:-/cvmfs/larsoft.opensciencegrid.org/spack-packages/opt/spack/linux-almalinux9-x86_64_v2/gcc-12.2.0/spdlog-1.12.0-dwa4wahktue5dmuyodu2bp2rjn6bomnw}"
GCC_DIR="${GCC_DIR:-/cvmfs/larsoft.opensciencegrid.org/spack-packages/opt/spack/linux-almalinux9-x86_64_v2/gcc-11.4.1/gcc-12.2.0-ojrjuib44dbyxvxgvwugxw77gsrkv3yc}"
SRCS_DIR="${SRCS_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"

LARCOREOBJ="$SRCS_DIR/larcoreobj"
SRC="$(dirname "${BASH_SOURCE[0]}")/12-installed-header-F3-checklist.cxx"

for p in "$SPDLOG_DIR/include" "$GCC_DIR/bin/g++" "$LARCOREOBJ/larcoreobj/LoggingUtil/Logging.h"; do
  if [[ ! -e "$p" ]]; then
    echo "ERROR: not found: $p"
    exit 1
  fi
done

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

echo "=============================================================="
echo "  F3 gate: installed header verification"
echo "=============================================================="
echo "  header:  $LARCOREOBJ/larcoreobj/LoggingUtil/Logging.h"
echo "  spdlog:  $SPDLOG_DIR"
echo "  gcc:     $("$GCC_DIR/bin/g++" --version | head -1)"
echo

"$GCC_DIR/bin/g++" -std=c++17 -Wall -Wextra -pedantic \
  -DSPDLOG_SHARED_LIB -DFMT_SHARED -DSPDLOG_COMPILED_LIB \
  -I"$LARCOREOBJ" -I"$SPDLOG_DIR/include" \
  "$SRC" "$LARCOREOBJ/larcoreobj/LoggingUtil/Logging.cxx" \
  -L"$SPDLOG_DIR/lib64" -lspdlog \
  -Wl,-rpath,"$SPDLOG_DIR/lib64" -Wl,-rpath,"$GCC_DIR/lib64" \
  -o "$OUT/verify" 2> "$OUT/build.err"

if [[ $? -ne 0 ]]; then
  echo "!! COMPILE FAILED:"
  sed 's/^/    /' "$OUT/build.err"
  exit 1
fi

if [[ -s "$OUT/build.err" ]]; then
  echo "!! COMPILED WITH WARNINGS (the header must be warning-clean):"
  sed 's/^/    /' "$OUT/build.err"
  exit 1
fi

SINK="$OUT/sink.log"

"$OUT/verify" "$SINK" > "$OUT/run.out" 2>&1
rc=$?
cat "$OUT/run.out"
echo

fail=0
check() { # name expected actual
  if [[ "$2" == "$3" ]]; then
    echo "  ok   $1 (expected $2, got $3)"
  else
    echo "  FAIL $1 (expected $2, got $3)"
    fail=1
  fi
}

echo "--- assertions ---"
check "program exit code" 0 "$rc"
check "CASE 12 move emits exactly once" 1 \
  "$(sed -n '/CASE 12/,/CASE 13/p' "$OUT/run.out" | grep -c 'emitted exactly once')"
check "CASE 13+14 emit nothing" 0 \
  "$(sed -n '/CASE 13/,/CASE 15/p' "$OUT/run.out" | grep -vc '^---')"
check "CASE 5 braces survive verbatim" 1 \
  "$(grep -c 'point { 0; 0; 0 }' "$OUT/run.out")"
check "CASE 6 no fmt error" 0 \
  "$(grep -c 'LOG ERROR' "$OUT/run.out")"
check "CASE 19 file sink written" 2 \
  "$(wc -l < "$SINK" | tr -d ' ')"

if (( fail )); then
  echo
  echo "F3 GATE: FAILED"
  exit 1
fi

echo
echo "F3 GATE: PASSED"
