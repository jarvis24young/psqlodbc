#!/bin/bash
# One-time WSL bootstrap for the psqlodbc coverage-DT pipeline.
# Idempotent: re-running is safe.
#
# Usage: bash wsl-bootstrap.sh [<windows-repo-path>]
#   windows-repo-path defaults to /mnt/d/GaussDB/psqlodbc
#
# Side effects:
#   - Creates ~/psqlodbc-build/ (CRLF-stripped copy of the repo)
#   - apt-installs build + test dependencies (requires sudo)
#   - Starts PostgreSQL and creates testuser / contrib_regression

set -euo pipefail

SRC="${1:-/mnt/d/GaussDB/psqlodbc}"
DST="$HOME/psqlodbc-build"

if [[ ! -d "$SRC" ]]; then
    echo "ERROR: source repo not found at $SRC" >&2
    echo "Usage: bash wsl-bootstrap.sh [<windows-repo-path>]" >&2
    exit 2
fi

echo "==> Copying $SRC to $DST (excluding .git, Debug, Release)"
mkdir -p "$DST"
rsync -a --exclude=".git" --exclude="Debug" --exclude="Release" "$SRC/" "$DST/"

echo "==> Stripping CRLF from source and build-system files"
find "$DST" -type f \( -name "*.sh" -o -name "bootstrap" \
    -o -name "configure.ac" -o -name "*.c" -o -name "*.h" \
    -o -name "*.am" -o -name "*.in" -o -name "Makefile*" \) -print0 \
  | xargs -0 sed -i 's/\r$//'

echo "==> Installing apt dependencies (sudo required)"
sudo apt-get update -y
sudo apt-get install -y \
    autoconf automake libtool \
    libpq-dev unixodbc unixodbc-dev libssl-dev \
    lcov \
    postgresql postgresql-contrib

echo "==> Ensuring PostgreSQL is running"
sudo service postgresql start
pg_lsclusters

echo "==> Creating test role and database (idempotent)"
sudo -u postgres psql <<SQL
DO \$\$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname='testuser') THEN
        CREATE ROLE testuser WITH LOGIN PASSWORD 'testpw' SUPERUSER;
    ELSE
        ALTER ROLE testuser WITH PASSWORD 'testpw';
    END IF;
END \$\$;
SQL

if ! sudo -u postgres psql -lqt | cut -d\| -f1 | grep -qw contrib_regression; then
    sudo -u postgres psql -c "CREATE DATABASE contrib_regression OWNER testuser;"
fi

echo "==> Smoke-testing libpq connectivity"
PGPASSWORD=testpw psql -h localhost -U testuser -d contrib_regression -c "SELECT 1;" \
    || { echo "ERROR: libpq smoke test failed — check pg_hba.conf" >&2; exit 3; }

echo ""
echo "=== WSL bootstrap complete ==="
echo "Build directory: $DST"
echo "DSN user/pw:     testuser / testpw"
echo "DB:              contrib_regression @ localhost:5432"
echo ""
echo "Next: bash build-with-gcov.sh <feature>"
