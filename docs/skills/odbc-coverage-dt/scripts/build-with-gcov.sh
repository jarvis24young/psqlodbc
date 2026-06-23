#!/bin/bash
# Build psqlodbc driver with gcov instrumentation, then build the
# specified test binary against it.
#
# Usage: bash build-with-gcov.sh <feature>
#   <feature> is the test name without the "-test" suffix,
#   e.g. "bracket-parse" for exe/bracket-parse-test

set -euo pipefail

FEATURE="${1:-}"
if [[ -z "$FEATURE" ]]; then
    echo "Usage: bash build-with-gcov.sh <feature>" >&2
    echo "Example: bash build-with-gcov.sh bracket-parse" >&2
    exit 2
fi

BUILD="$HOME/psqlodbc-build"
if [[ ! -d "$BUILD" ]]; then
    echo "ERROR: $BUILD not found. Run wsl-bootstrap.sh first." >&2
    exit 2
fi

cd "$BUILD"

echo "==> bootstrap (autoreconf)"
./bootstrap 2>&1 | tail -n 3

echo "==> configure with gcov + SQLCOLATTRIBUTE_SQLLEN"
./configure \
    CFLAGS="-O0 -g -fprofile-arcs -ftest-coverage -I/usr/include/postgresql -DSQLCOLATTRIBUTE_SQLLEN" \
    LDFLAGS="--coverage" \
    --with-unixodbc=/usr --with-libpq=/usr \
    2>&1 | tail -n 5

echo "==> make driver (-j4)"
make -j4 2>&1 | tail -n 3

if [[ ! -f "$BUILD/.libs/psqlodbcw.so" ]]; then
    echo "ERROR: psqlodbcw.so not built" >&2
    exit 3
fi

echo "==> Verifying gcno files"
GCNO_COUNT=$(find "$BUILD/.libs" -name "*.gcno" | wc -l)
if [[ "$GCNO_COUNT" -eq 0 ]]; then
    echo "ERROR: no .gcno files — gcov instrumentation missing" >&2
    exit 3
fi
echo "    $GCNO_COUNT .gcno files present"

echo "==> Generating test ODBC config"
cd "$BUILD/test"
chmod +x odbcini-gen.sh
./odbcini-gen.sh >/dev/null

# odbcini-gen.sh leaves DSN credentials blank — write a correct odbc.ini
cat > odbc.ini <<EOF
[psqlodbc_test_dsn]
Description = psqlodbc test DSN
Driver      = PostgreSQL Unicode
Database    = contrib_regression
Servername  = localhost
Username    = testuser
Password    = testpw
Port        = 5432
ReadOnly    = No
EOF

echo "==> Checking registration of exe/${FEATURE}-test in test/tests"
if ! grep -q "exe/${FEATURE}-test" tests; then
    echo "WARN: exe/${FEATURE}-test not found in test/tests." >&2
    echo "      Append it (with a trailing backslash on the previous line) then re-run." >&2
    exit 4
fi

echo "==> make test binary with LIBODBC=-lodbc"
make LIBODBC="-lodbc" "exe/${FEATURE}-test" 2>&1 | tail -n 5

if [[ ! -x "$BUILD/test/exe/${FEATURE}-test" ]]; then
    echo "ERROR: exe/${FEATURE}-test not produced" >&2
    exit 3
fi

echo ""
echo "=== Build complete ==="
echo "Driver:        $BUILD/.libs/psqlodbcw.so"
echo "Test binary:   $BUILD/test/exe/${FEATURE}-test"
echo ""
echo "Next: bash run-coverage.sh ${FEATURE}"
