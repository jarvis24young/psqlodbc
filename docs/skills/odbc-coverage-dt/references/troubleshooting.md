# Troubleshooting Index

Failure → diagnosis → fix, indexed for fast lookup.

## Build phase

### `./bootstrap: cannot execute: required file not found`
**Diagnosis**: CRLF line endings on the shebang line.
**Fix**: Copy repo to WSL native FS and strip `\r` — see `wsl-setup.md` step 1.

### `configure: error: libpq header not found`
**Diagnosis**: libpq-dev installed but psqlodbc's configure doesn't probe `/usr/include/postgresql`.
**Fix**: Add `-I/usr/include/postgresql` to CFLAGS; optionally `--with-libpq=/usr`.

### `conflicting types for 'SQLColAttribute'` at `odbcapi30.c:120`
**Diagnosis**: unixODBC 2.3+ on x86_64 declares `SQLColAttribute`'s last param as `SQLLEN *`, but psqlodbc's default is `SQLPOINTER`.
**Fix**: Add `-DSQLCOLATTRIBUTE_SQLLEN` to CFLAGS.

### `undefined reference to SQLFreeHandle` when linking test binary
**Diagnosis**: `test/Makefile` has `LIBODBC=` empty; configure didn't find libodbc on Ubuntu.
**Fix**: `make LIBODBC="-lodbc" exe/<feature>-test` instead of plain `make`.

### `error while loading shared libraries: libpq.so.5`
**Diagnosis**: Dynamic linker can't find libpq at runtime.
**Fix**: `export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH`

## Runtime phase

### All TCs fail with `SQLSTATE=08001 password authentication failed`
**Diagnosis**: Either the `odbc.ini` DSN has blank `Username`/`Password` (common after `odbcini-gen.sh`) or the PG role password doesn't match.
**Fix**: Write odbc.ini manually — see stage 6 in SKILL.md. Verify with `isql -v <dsn> user pw <<< "SELECT 1;"` before running the test binary.

### Test binary runs but gcov shows 0 hits
**Diagnosis**: The test is linking against a system-installed `psqlodbcw.so` (no gcov instrumentation) instead of your build's `.so`.
**Fix**: Confirm `odbcinst.ini`'s `Driver =` path points at `~/psqlodbc-build/.libs/psqlodbcw.so`. Re-run `test/odbcini-gen.sh` to regenerate — it writes the correct relative path automatically.

### TC reaches PG auth but target branch shows 0 hits
**Diagnosis**: The connection string triggers a different (earlier) branch in the switch than you expected.
**Fix**: Add `printf("DEBUG: entering branch X\n");` (or read gcov output per line) to confirm which path ran. Usually means a prior token has already consumed the state your TC depends on; rearrange the string.

## Coverage phase

### `gcov` says "No executable lines" for the target file
**Diagnosis**: The `.gcda` isn't in the same directory as the `.gcno`, or the build was done without `-fprofile-arcs -ftest-coverage`.
**Fix**: `find ~/psqlodbc-build -name "*drvconn*.gc*"` should show matched `.gcno` + `.gcda` in `.libs/`. If only `.gcno` exists, the test binary didn't load the instrumented .so — see "gcov shows 0 hits" above.

### `lcov` reports `branches...: no data found`
**Diagnosis**: Branch-coverage is opt-in at both capture and genhtml stages.
**Fix**: Pass `--branch-coverage --rc branch_coverage=1` to **both** `lcov --capture` and `genhtml`.

### `genhtml` produces pages but drvconn.c isn't there
**Diagnosis**: lcov --extract pattern didn't match.
**Fix**: Use a glob, not a literal: `lcov --extract coverage.info "*/drvconn.c" ...` (note the `*/`).

## WSL-specific

### `wsl: 检测到 localhost 代理配置，但未镜像到 WSL`
Cosmetic. Ignore.

### `sudo: a password is required` in a background Bash task
**Diagnosis**: Background tasks have no stdin, so `sudo -v` can't prompt.
**Fix**: Either pipe with `echo "pw" | sudo -S -p "" <cmd>` (not recommended for security), or have the user run the sudo step in a separate Ubuntu terminal outside Claude.

### PG cluster is down after WSL restart
**Diagnosis**: WSL's PostgreSQL doesn't autostart.
**Fix**: `sudo service postgresql start`. Consider adding to `~/.bashrc` or WSL's systemd boot.

## Registration phase

### `make exe/<feature>-test` says "No rule to make target"
**Diagnosis**: Either the source file name doesn't match (`src/<feature>-test.c` required) or the binary isn't in `test/tests` (Makefile reads this file).
**Fix**: Check both. The `tests` file must have the line `exe/<feature>-test` with a trailing backslash on prior lines.

### Python-based sed replacement mangles the `tests` file
**Diagnosis**: Shell-level escaping ate the backslash.
**Fix**: Use the Edit tool on the file directly, or open it in an editor. Don't fight sed's escaping.

## When unsure

Run these four commands; they reveal 80% of issues:

```bash
file ~/psqlodbc-build/bootstrap       # should say "POSIX shell script, ASCII text executable" (no CRLF)
find ~/psqlodbc-build -name "*.gcno" | wc -l   # should be >0 after make
cat ~/psqlodbc-build/test/odbc.ini    # Username/Password must NOT be blank
ldd ~/psqlodbc-build/.libs/psqlodbcw.so | grep -E "libpq|libodbc"  # sanity
```
