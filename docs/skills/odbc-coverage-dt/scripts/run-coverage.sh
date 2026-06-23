#!/bin/bash
# Run the test binary and produce gcov + lcov HTML coverage reports.
#
# Usage: bash run-coverage.sh <feature> [<targetfile>]
#   <feature>    test name (e.g. "bracket-parse")
#   <targetfile> source file to report on (default: inferred from gcno, first hit)
#
# Output:
#   ~/psqlodbc-build/<targetfile>.c.gcov      (terminal-readable)
#   ~/psqlodbc-build/coverage-html/           (browser-viewable)
#   /mnt/d/GaussDB/psqlodbc/test/docs/coverage-html/  (mirrored to Windows)

set -euo pipefail

FEATURE="${1:-}"
TARGET="${2:-}"

if [[ -z "$FEATURE" ]]; then
    echo "Usage: bash run-coverage.sh <feature> [<targetfile>]" >&2
    echo "Example: bash run-coverage.sh bracket-parse drvconn" >&2
    exit 2
fi

BUILD="$HOME/psqlodbc-build"
BIN="$BUILD/test/exe/${FEATURE}-test"

if [[ ! -x "$BIN" ]]; then
    echo "ERROR: $BIN not found. Run build-with-gcov.sh $FEATURE first." >&2
    exit 2
fi

# Clear old gcda to get a clean run
find "$BUILD/.libs" -name "*.gcda" -delete

echo "==> Running $BIN"
cd "$BUILD/test"
ODBCSYSINI=. ODBCINSTINI=./odbcinst.ini ODBCINI=./odbc.ini \
    "$BIN" 2>&1 | tee "${FEATURE}-run.log"

if ! grep -q "All tests completed without crash" "${FEATURE}-run.log"; then
    echo "WARN: test run did not print the final completion banner — something crashed" >&2
fi

# Infer target if not supplied: pick the gcno that was most recently written (= hit)
if [[ -z "$TARGET" ]]; then
    TARGET=$(find "$BUILD/.libs" -name "*.gcda" -printf "%f\n" \
        | sed 's/psqlodbcw_la-//;s/\.gcda$//' \
        | head -n1)
    if [[ -z "$TARGET" ]]; then
        echo "ERROR: could not infer target file from .gcda output" >&2
        exit 3
    fi
    echo "==> Inferred target: ${TARGET}.c"
fi

cd "$BUILD"

echo "==> gcov -b -c (branch coverage)"
gcov -b -c -o .libs ".libs/psqlodbcw_la-${TARGET}.o" 2>&1 | tail -n 8

echo "==> lcov capture"
lcov --capture --directory .libs --output-file coverage.info \
     --rc geninfo_unexecuted_blocks=1 --rc branch_coverage=1 \
     --branch-coverage 2>&1 | tail -n 3

echo "==> lcov extract ${TARGET}.c"
lcov --extract coverage.info "*/${TARGET}.c" --output-file "${TARGET}.info" \
     --rc branch_coverage=1 --branch-coverage 2>&1 | tail -n 3

echo "==> genhtml"
rm -rf coverage-html
genhtml "${TARGET}.info" --output-directory coverage-html \
     --branch-coverage --rc branch_coverage=1 \
     --title "${TARGET}.c ${FEATURE} DT coverage" 2>&1 | tail -n 10

# Mirror to Windows side for browser viewing
WIN_DEST="/mnt/d/GaussDB/psqlodbc/test/docs/coverage-html"
if [[ -d "/mnt/d/GaussDB/psqlodbc" ]]; then
    rm -rf "$WIN_DEST"
    cp -r coverage-html "$WIN_DEST"
    echo ""
    echo "=== Coverage report ready ==="
    echo "Windows:  D:\\GaussDB\\psqlodbc\\test\\docs\\coverage-html\\index.html"
fi

echo "Terminal: less $BUILD/${TARGET}.c.gcov"
echo ""
echo "Summary:"
grep -E "^Lines|^Branches|^Functions" "${TARGET}.info" 2>/dev/null || true
tail -n 6 coverage-html/index.html 2>/dev/null \
    | grep -oE '[0-9]+\.[0-9]+%' | head -n 4 \
    | awk 'BEGIN{split("line branch func",L)} {print L[NR]": "$0}' 2>/dev/null || true
