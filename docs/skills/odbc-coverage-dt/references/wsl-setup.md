# WSL + PostgreSQL + unixODBC Setup

One-time bootstrap for a machine that will run the coverage pipeline. Idempotent — re-running is safe.

## Prerequisites

- Windows 11 with WSL2 + Ubuntu 24.04 installed
- Windows-side psqlodbc checkout at `D:\GaussDB\psqlodbc` (adjust paths in scripts if different)
- Git configured on Windows side (fork remote optional)

## Steps

### 1. Copy repo to WSL native filesystem

Windows mounts preserve CRLF line endings, which breaks `#!/bin/sh` shebangs. The build must happen on a Linux-native filesystem.

```bash
rsync -a --exclude=".git" --exclude="Debug" --exclude="Release" \
    /mnt/d/GaussDB/psqlodbc/ ~/psqlodbc-build/

find ~/psqlodbc-build -type f \( -name "*.sh" -o -name "bootstrap" \
    -o -name "configure.ac" -o -name "*.c" -o -name "*.h" \
    -o -name "*.am" -o -name "*.in" -o -name "Makefile*" \) -print0 \
  | xargs -0 sed -i 's/\r$//'
```

### 2. Install build dependencies

```bash
sudo apt-get update
sudo apt-get install -y autoconf automake libtool \
    libpq-dev unixodbc unixodbc-dev libssl-dev lcov \
    postgresql postgresql-contrib
```

Verify:

```bash
command -v psql pg_config odbcinst isql gcc make lcov genhtml
```

All 8 should resolve.

### 3. Start PostgreSQL

Ubuntu 24.04 with systemd-for-WSL:

```bash
sudo service postgresql start
pg_lsclusters    # should show '16 main 5432 online'
```

If the port is not 5432, remember the actual port for Step 5.

### 4. Create test role and database

```bash
sudo -u postgres psql -c "CREATE ROLE testuser WITH LOGIN PASSWORD 'testpw' SUPERUSER;"
sudo -u postgres psql -c "CREATE DATABASE contrib_regression OWNER testuser;"
```

If the role already exists, `ALTER ROLE testuser WITH PASSWORD 'testpw';` to reset.

### 5. Smoke-test libpq connectivity

```bash
PGPASSWORD=testpw psql -h localhost -U testuser -d contrib_regression -c "SELECT 1;"
```

Should print `1`. If it hangs, check `pg_hba.conf` allows md5/scram-sha-256 from 127.0.0.1.

## Output of this stage

- `~/psqlodbc-build/` with CRLF-stripped sources
- PostgreSQL 16 running on `localhost:5432`
- Role `testuser` / `testpw` owning `contrib_regression`

Proceed to `references/build-flags.md` for Stage 5.
