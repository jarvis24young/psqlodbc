# Bug Watch Tests

These tests are intentionally aimed at confirming the currently observed bugs.

There are two kinds:

- white-box bug-watch tests: complete normally and print `BUG:` diagnostics
- ASan bug-watch tests: are expected to abort with AddressSanitizer while the bug exists

## White-box bug-watch tests

### 1. `dlg_specific.c` input mutation

File:

- `test/src/dlg-specific-whitebox-test.c`

What it watches:

- `copyConnAttributes()` mutates the caller buffer for `Protocol=7.4-1`

Current confirmation line:

- `BUG: copyConnAttributes mutated caller input buffer from '7.4-1' to '7.4'`

### 2. `convert.c` interval parsing rejection

File:

- `test/src/convert-whitebox-test.c`

What it watches:

- `interval2istruct()` rejects valid interval texts

Current confirmation lines:

- `BUG: interval2istruct rejected valid day-second text '-4 day 05:06:07.987654'`
- `BUG: interval2istruct rejected valid hour-second text '-05:06:07.5'`

### 3. `parse.c` alias detection

File:

- `test/src/parse-whitebox-test.c`

What it watches:

- `include_alias_wo_as()` treats operator-only predecessor tokens as aliasable

Current confirmation line:

- `BUG: include_alias_wo_as treats operator-only btoken '+' as aliasable; expected FALSE`

## ASan bug-watch tests

### 4. `convert.c::getPrecisionPart()`

File:

- `test/src/convert-asan-bugwatch-test.c`

What it watches:

- unchecked `precision` writes past local `fraction[]`

Expected current outcome:

- ASan `stack-buffer-overflow` in `getPrecisionPart`

### 5. `parse.c::getNextToken()`

File:

- `test/src/parse-asan-bugwatch-test.c`

What it watches:

- leading quoted token can read `tstr[-1]`

Expected current outcome:

- ASan invalid read / `stack-buffer-overflow` near `getNextToken`

### 6. `convert.c` real black-box interval precision repro

File:

- `test/src/convert-ard-blackbox-asan-test.c`

What it watches:

- real ODBC path `SQLGetData(SQL_ARD_TYPE)` with ARD precision override
- call chain: `PGAPI_GetData()` -> `copy_and_convert_field()` -> `interval2istruct()` -> `getPrecisionPart()`

Current confirmed behavior:

- baseline mode (no extra argument) succeeds against the live DSN and prints:
  - `server interval text: 4 5:06:07.123456`
  - `running blackbox interval fetch with precision=6`
  - `fraction=123456`
- repro mode (`20`) crashes under ASan on the real path at `convert.c:617` in `interval2istruct`

## Re-run commands in WSL

White-box:

```bash
cd ~/psqlodbc-build/test
./exe/dlg-specific-whitebox-test
./exe/convert-whitebox-test
./exe/parse-whitebox-test
```

ASan:

```bash
cd ~/psqlodbc-build/test
make LIBODBC="-lodbc" \
  CFLAGS_ADD="-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -ffunction-sections -fdata-sections" \
  LDFLAGS="-fsanitize=address,undefined -Wl,--gc-sections" \
  exe/convert-asan-bugwatch-test exe/parse-asan-bugwatch-test \
  exe/convert-ard-blackbox-asan-test

ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 ./exe/convert-asan-bugwatch-test
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 ./exe/parse-asan-bugwatch-test
ODBCSYSINI=. ODBCINSTINI=./odbcinst.ini ODBCINI=./odbc.ini \
  ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 \
  ./exe/convert-ard-blackbox-asan-test
ODBCSYSINI=. ODBCINSTINI=./odbcinst.ini ODBCINI=./odbc.ini \
  ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 \
  ./exe/convert-ard-blackbox-asan-test 20
```

## How to use after fixing

These are bug-watch tests, not final regression assertions.

After the implementation is fixed:

1. the white-box tests should stop printing the `BUG:` diagnostics
2. the ASan repro binaries should stop crashing
3. at that point the tests should be inverted into normal regression expectations
