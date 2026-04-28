# Issue #149 convert_to_pgbinary DT notes

GitHub issue: https://github.com/postgresql-interfaces/psqlodbc/issues/149

## Bug explanation

Issue #149 reported an unsigned-length error in `convert.c::convert_to_pgbinary()`.
On PostgreSQL 9.0+ the driver uses hex bytea text format. In that path,
`convert_to_pgbinary()` prefixes the output with `\x` and then calls
`pg_bin2hex()`:

```c
out[o++] = '\\';
out[o++] = 'x';
o += pg_bin2hex(in, out + o, len);
return o;
```

`pg_bin2hex()` can return `-1` when it detects a dangerous input/output buffer
overlap. Before PR #150, that `-1` was added to `o`, whose type is `size_t`.
That could turn an internal conversion failure into a huge unsigned length and
let the caller continue as if conversion had succeeded.

PR #150 fixed this by storing the return value in a signed `SQLLEN`, checking
for `-1`, and propagating the error before updating `o`.

## Black-box reachability note

The normal public ODBC path reaches `convert_to_pgbinary()` by binding a binary
parameter as `SQL_C_BINARY` / `SQL_VARBINARY` while server-side prepare is
disabled. That is what the GTest DT covers.

The exact `pg_bin2hex() == -1` overlap branch is an internal buffer-overlap
condition. A normal ODBC application does not control the driver's internal
query buffer address, so a strict black-box test cannot reliably force that
specific branch. A direct source-level/white-box test is required if branch
coverage for that exact error arm is mandatory.

## Added DT file

- `test/src/convert-pgbinary-issue149-dt.cpp`

The tests use the requested GTest ODBC fixture style and connect with:

```text
DSN=ODBC_DT_A;UseServerSidePrepare=0
```

`UseServerSidePrepare=0` is important because otherwise bytea parameters can be
sent through libpq binary parameters and bypass the text conversion path under
test.

## Covered behavior

1. `ConvertToPgbinaryHexPathShouldRoundTripBinary_L0`
   - Binds bytes containing NUL, quote, backslash, `0x80`, and `0xff`.
   - Verifies PostgreSQL receives the exact bytes via `encode(?::bytea, 'hex')`.

2. `ConvertToPgbinaryShouldNotCorruptFollowingParameters_L0`
   - Places integer parameter after two binary parameters.
   - Verifies binary conversion length does not corrupt following parameters.

3. `ConvertToPgbinaryShouldHandleLargeBinaryAndExpandQueryBuffer_L0`
   - Uses a 1024-byte binary parameter.
   - Exercises query-buffer expansion around `CVT_APPEND_BINARY`.
