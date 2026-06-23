---
name: odbc-coverage-dt
description: End-to-end pipeline for raising branch coverage on ODBC driver C source (psqlodbc, GaussDB ODBC, any unixODBC-compatible driver) via Design Tests. Use this skill whenever the user asks to analyze an ODBC driver function for uncovered branches, author crash-hardening test cases, wire tests into psqlodbc's test/ Makefile, run tests against a live PostgreSQL through unixODBC, or produce gcov/lcov coverage reports — even if they only mention "提升覆盖率" / "写测试" / "测一下这个函数会不会 crash" / "跑一下 ODBC 测试" without naming the full pipeline. Also use for related tasks like setting up a WSL build environment for psqlodbc, decoding unixODBC build errors, or authoring coverage reports in the project's test/docs style.
---

# ODBC Driver Coverage-Driven DT Pipeline

A repeatable workflow for hardening ODBC driver source code. The pipeline exercises the **real** driver through unixODBC + PostgreSQL in WSL (not a mock) while measuring branch coverage with gcov, so every test case's contribution is verifiable.

## When this skill fires

Symptom patterns that should invoke this skill:

- User names a function in an ODBC driver (drvconn, dlg_specific, convert, results, statement, etc.) and wants test cases for it
- User asks "how do I know my tests actually exercise branch X"
- User hits unixODBC-specific build errors (`SQLColAttribute` signature clash, `odbcini-gen.sh` blank placeholders, `LIBODBC=` empty)
- User wants to migrate a Windows-developed test to run under WSL
- User talks about "DT / 设计测试 / 分支覆盖 / gcov / lcov" in an ODBC context

If the user only wants to run an existing test suite with no coverage goal, a plain `make installcheck` is enough — skip the skill.

## Core philosophy

1. **Stability-first DT.** The pass criterion is "no crash / no segfault / no stack overflow" — not SQL success. A test case that reaches "connection refused" still proves the parse code survived the input. This is appropriate for hardening tasks (加固) where the goal is defensive coverage of edge cases.

2. **Measure, don't assume.** Claims like "this test covers branch B3a" are worthless without a gcov number. Always run the coverage tooling and map hits back to a branch matrix before declaring done.

3. **Keep the feedback loop tight.** After the first run, the uncovered branches tell you exactly which inputs you still need to craft. Iterate.

## Workflow (9 stages)

```
1. Analyze   → read source, enumerate branches B0..Bn
2. Design    → map each TC 1:1 to a branch in test/src/<feature>-test.c
3. Register  → append exe/<feature>-test to test/tests with trailing backslash
4. WSL setup → copy to native FS, strip CRLF, install deps, start PG
5. Build     → configure driver with gcov flags, make
6. DSN       → write odbc.ini manually (odbcini-gen.sh leaves blanks)
7. Run       → execute test binary, confirm 20/20 "[No crash - PASS]"
8. Measure   → gcov + lcov/genhtml, map hits to branch matrix
9. Iterate   → author new TCs for uncovered branches until ≥95% or justified
```

Each stage is documented separately. Read the matching reference file only when you reach that stage — they're too long to load all at once.

## Reference files (load on demand)

| File | When to read |
|------|-------------|
| `references/wsl-setup.md` | Stage 4 — one-time WSL + PG + unixODBC bootstrap |
| `references/build-flags.md` | Stage 5 — configure/make flags, common build errors |
| `references/test-template.md` | Stage 2 — how to structure a `<feature>-test.c` |
| `references/analysis-template.md` | Stage 1 — the MD analysis doc layout |
| `references/coverage-report-template.md` | Stage 8 — the MD coverage report layout |
| `references/troubleshooting.md` | Any stage — indexed failure-mode → fix table |

## Bundled scripts (run verbatim)

| Script | Purpose |
|--------|---------|
| `scripts/wsl-bootstrap.sh` | Idempotent: copy repo to WSL, strip CRLF, install apt deps, create test role + DB. Run once per machine. |
| `scripts/build-with-gcov.sh <feature>` | Configure + make driver with coverage flags, build `exe/<feature>-test`. |
| `scripts/run-coverage.sh <feature>` | Run the test binary, produce gcov + lcov HTML in `coverage-html/`, copy to Windows mount. |

Invocation pattern:

```bash
bash scripts/wsl-bootstrap.sh                      # once
bash scripts/build-with-gcov.sh bracket-parse      # per feature
bash scripts/run-coverage.sh bracket-parse         # per iteration
```

The scripts fail loudly and print the exact reproduction command on error. Read them once when you first run the workflow so you understand what they do — afterwards invoke them without re-reading.

## Why each stage matters (the non-obvious parts)

- **Stage 4 copy to native FS**: Windows mount `/mnt/d/...` preserves CRLF. A `#!/bin/sh\r` shebang makes `./bootstrap` fail with a misleading "cannot execute: required file not found". The copy is the only reliable fix; dos2unix on the Windows mount will corrupt the user's in-progress work.
- **Stage 5 `-DSQLCOLATTRIBUTE_SQLLEN`**: unixODBC 2.3+ on x86_64 declares `SQLColAttribute`'s last param as `SQLLEN *`, but psqlodbc's `odbcapi30.c` defaults to `SQLPOINTER`. The driver already has the right conditional — you just need to flip it on.
- **Stage 6 manual `odbc.ini`**: `odbcini-gen.sh` has an undocumented requirement that DSN fields come from command-line args; ignoring this produces a valid-looking ini with empty `Servername=` / `Username=` that silently fails authentication with a misleading SQLSTATE. Write it by hand.
- **Stage 7 errors are features**: error-path TCs (TC06/08 style) SHOULD return `SQLSTATE 08001 "Connection string parse error"` — that's the driver correctly rejecting malformed input, which is exactly what we wanted to verify. Count them as PASS.
- **Stage 8 `--branch-coverage` on both calls**: lcov's branch data is opt-in at both capture and genhtml time. Passing the flag only to genhtml produces the infamous "no branches found" with no explanation.

## Reference implementation

This skill was distilled from a live session that covered `drvconn.c::dconn_get_attributes()` in the psqlodbc repo. The concrete artifacts live at:

- `test/docs/bracket-parse-analysis.md` — Stage 1 output (branch enumeration + risk analysis)
- `test/src/bracket-parse-test.c` — Stage 2 output (20-TC harness)
- `test/docs/bracket-parse-coverage-report.md` — Stage 8 output (72.79% line, 72.22% branch, 0 crashes, 3 branches remaining)
- `test/docs/coverage-html/` — Stage 8 HTML report

When starting a new feature, copy the structure of these three markdown files and replace the content, rather than re-inventing the sections.

## Example invocations

**Example 1: Fresh target function**

> User: 帮我给 `decode_or_remove_braces` 在 dlg_specific.c 里写 DT，看看覆盖率

Expected flow:
1. Read `dlg_specific.c`, find the function, enumerate branches → `test/docs/decode-braces-analysis.md`
2. Author `test/src/decode-braces-test.c` with TC per branch
3. Register in `test/tests`
4. Run `scripts/build-with-gcov.sh decode-braces` + `scripts/run-coverage.sh decode-braces`
5. Write `test/docs/decode-braces-coverage-report.md` with hit matrix
6. Present uncovered branches + proposed follow-up TCs

**Example 2: Iteration on existing skill output**

> User: bracket-parse 的 B2a 和 B3a 还没覆盖，补两个用例吧

Expected flow:
1. Re-read the analysis doc's B2a / B3a preconditions
2. Construct conn strings that force those preconditions (usually requires a *prior* token to set state)
3. Append TC21, TC22 to `bracket-parse-test.c`
4. `scripts/run-coverage.sh bracket-parse`
5. Diff the new gcov against the old report; update the report's branch matrix

**Example 3: Build troubleshooting**

> User: 我在 Ubuntu 上 configure 之后 make 报 `SQLColAttribute` 冲突

Expected flow:
1. Recognize this as the `-DSQLCOLATTRIBUTE_SQLLEN` case from `references/troubleshooting.md`
2. Rerun `./configure` with the define added to CFLAGS
3. `make clean && make -j4`

## Anti-patterns to avoid

- **Don't mock the ODBC layer.** The whole point is to exercise real parsing + real libpq + real TCP; mocking defeats the skill.
- **Don't declare done on line coverage.** Branch coverage is what matters; a line can be hit by only one direction of its if/else.
- **Don't skip the "why" in coverage reports.** Future-you (or the next engineer) needs to know which uncovered branches are unreachable by design vs. missing TC. Document both.
- **Don't commit `~/psqlodbc-build/` back to the Windows-mounted repo.** It's a build working directory, not a source. Only commit `test/src/<feature>-test.c`, the two `test/docs/*.md` files, the HTML report directory, and the `test/tests` registration line.

## Extensions (optional, activate on request)

- **ASan + UBSan** pass — same pipeline, swap the configure flags per `references/build-flags.md#sanitizer-build`
- **Valgrind memcheck** — post-run check for leaks / uninitialized reads
- **Stress amplifier** — for-loop to catch flaky crashes
- **libFuzzer wrapper** — for genuinely random input exploration around the same function

These are documented in `references/build-flags.md` under the "Hardening extensions" section. Only invoke when the user asks.

## Success criteria

The skill has done its job when:

1. Every TC outputs `[No crash - PASS]` and the run ends with `=== All tests completed without crash ===`
2. `drvconn.c.gcov` (or the target file) shows a branch coverage %
3. The coverage report MD contains a complete TC-to-branch mapping table
4. Remaining uncovered branches are either (a) slated for follow-up TCs or (b) justified as unreachable
5. The HTML report is viewable from the Windows side at `test/docs/coverage-html/index.html`

If any of these is missing, the workflow hasn't finished — don't declare done.
