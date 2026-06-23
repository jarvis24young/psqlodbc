# Build Flags for Coverage + Hardening

## Coverage build (default)

```bash
cd ~/psqlodbc-build
./bootstrap
./configure \
  CFLAGS="-O0 -g -fprofile-arcs -ftest-coverage -I/usr/include/postgresql -DSQLCOLATTRIBUTE_SQLLEN" \
  LDFLAGS="--coverage" \
  --with-unixodbc=/usr --with-libpq=/usr
make -j4
```

### Flag rationale

| Flag | Why |
|------|-----|
| `-O0 -g` | Debuggable line info, no optimization-induced line merging (gcov becomes misleading at -O2) |
| `-fprofile-arcs` | Emit `.gcno` at compile time |
| `-ftest-coverage` | Emit `.gcda` at runtime |
| `-I/usr/include/postgresql` | libpq-dev installs headers under this prefix, but psqlodbc's configure may not find them automatically |
| `-DSQLCOLATTRIBUTE_SQLLEN` | **Required on Ubuntu x86_64.** unixODBC 2.3+ declares `SQLColAttribute`'s last param as `SQLLEN *`, but `odbcapi30.c` defaults to `SQLPOINTER`. Without this define, build fails with `conflicting types for 'SQLColAttribute'`. |
| `LDFLAGS="--coverage"` | Links `libgcov.a` into the .so so runtime can emit .gcda |
| `--with-unixodbc=/usr --with-libpq=/usr` | Points to system installs rather than custom builds |

### Expected build output

```
...
libtool: link: gcc -shared ... -o .libs/psqlodbcw.so
make[1]: Leaving directory '/home/yjw/psqlodbc-build'
```

And `find ~/psqlodbc-build -name "*.gcno" | head` should show `.libs/psqlodbcw_la-drvconn.gcno` etc.

## Test binary build

The test Makefile's `LIBODBC` variable is blank by default — `configure` doesn't detect it on Ubuntu. You must override at make time:

```bash
cd ~/psqlodbc-build/test
make LIBODBC="-lodbc" exe/<feature>-test
```

Without `LIBODBC="-lodbc"`, linking fails with `undefined reference to SQLFreeHandle` etc.

## Sanitizer build (ASan + UBSan)

Use when the coverage pass is clean but you want to catch UB / memory errors:

```bash
./configure \
  CFLAGS="-O1 -g -fno-omit-frame-pointer \
          -fsanitize=address,undefined \
          -fprofile-arcs -ftest-coverage \
          -I/usr/include/postgresql -DSQLCOLATTRIBUTE_SQLLEN" \
  LDFLAGS="-fsanitize=address,undefined --coverage" \
  --with-unixodbc=/usr --with-libpq=/usr
make clean && make -j4
```

Run with:

```bash
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
ODBCSYSINI=. ODBCINSTINI=./odbcinst.ini ODBCINI=./odbc.ini \
    ./exe/<feature>-test
```

ASan errors appear on stderr as `==PID==ERROR: AddressSanitizer: ...` with a full stack trace.

## Valgrind build

Valgrind wants normal (non-sanitized) builds. Same coverage flags work:

```bash
valgrind --error-exitcode=1 --leak-check=full --track-origins=yes \
    ./exe/<feature>-test
```

## Common build errors

| Error | Fix |
|-------|-----|
| `bootstrap: cannot execute: required file not found` | CRLF on shebang — redo the rsync + sed from wsl-setup.md |
| `conflicting types for 'SQLColAttribute'` | Add `-DSQLCOLATTRIBUTE_SQLLEN` to CFLAGS |
| `libpq-fe.h: No such file` | Add `-I/usr/include/postgresql` to CFLAGS |
| `undefined reference to SQLFreeHandle` at test link | `make LIBODBC="-lodbc"` |
| `error while loading shared libraries: libpq.so` at test run | `export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH` |
| configure warnings about AC_LIBTOOL_DLOPEN / AC_HEADER_TIME | Cosmetic, ignore |

## Hardening extensions summary

| Mode | CFLAGS addition | LDFLAGS addition | When |
|------|----------------|------------------|------|
| Coverage | `-fprofile-arcs -ftest-coverage` | `--coverage` | Always |
| ASan+UBSan | `-fsanitize=address,undefined -fno-omit-frame-pointer` | `-fsanitize=address,undefined` | After coverage passes |
| Stress loop | none, just run in a bash for-loop 1000× | | Flaky-crash hunting |
| libFuzzer | `-fsanitize=fuzzer,address` | `-fsanitize=fuzzer,address` | Input-space exploration |
