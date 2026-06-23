# Test Harness Template

Every DT test for an ODBC driver function follows the same shape. This file captures the skeleton.

## File location and naming

- `test/src/<feature>-test.c`
- Use kebab-case for the feature name (`bracket-parse`, `decode-braces`, `cursor-scroll`)
- Binary name becomes `exe/<feature>-test`

## Skeleton

```c
/*
 * Test cases for <feature> in <targetfile>.c
 * Targets: <targetfunction>()
 *
 * These tests exercise <what> to verify robustness against
 * crashes/core dumps (stability-first DT).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * Helper: attempt the ODBC call with given input.
 * Returns SQLRETURN. Does NOT exit on failure.
 */
static SQLRETURN
try_connect(const char *connstr, const char *test_label)
{
	SQLRETURN ret;
	SQLCHAR outstr[1024];
	SQLSMALLINT outlen;
	SQLHENV local_env = SQL_NULL_HENV;
	SQLHDBC local_conn = SQL_NULL_HDBC;

	printf("Test: %s\n", test_label);
	printf("  ConnStr: %s\n", connstr);

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &local_env);
	SQLSetEnvAttr(local_env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, local_env, &local_conn);

	ret = SQLDriverConnect(local_conn, NULL,
	                       (SQLCHAR *)connstr, SQL_NTS,
	                       outstr, sizeof(outstr), &outlen,
	                       SQL_DRIVER_NOPROMPT);

	if (SQL_SUCCEEDED(ret))
	{
		printf("  Result: Connected OK\n");
		SQLDisconnect(local_conn);
	}
	else
	{
		SQLCHAR sqlstate[6], msg[1024];
		SQLINTEGER native_err;
		SQLSMALLINT msglen;

		SQLGetDiagRec(SQL_HANDLE_DBC, local_conn, 1,
		              sqlstate, &native_err, msg, sizeof(msg), &msglen);
		printf("  Result: Failed (SQLSTATE=%s): %s\n", sqlstate, msg);
	}

	SQLFreeHandle(SQL_HANDLE_DBC, local_conn);
	SQLFreeHandle(SQL_HANDLE_ENV, local_env);

	printf("  [No crash - PASS]\n\n");
	return ret;
}

/*
 * TCxx: <description>
 * Covers: <branch ID from analysis doc>
 */
static void
test_<descriptive_name>(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
	         "DSN=%s;<attribute>={<value>}", get_test_dsn());
	try_connect(connstr, "TCxx: <description>");
}

int main(int argc, char **argv)
{
	printf("=== <Feature> Test Suite for <targetfile>.c ===\n");
	printf("Target: <targetfunction>()\n\n");

	test_<descriptive_name>();   /* TCxx */
	/* ... more TCs ... */

	printf("=== All tests completed without crash ===\n");
	return 0;
}
```

## Invariants for every TC

1. **Local handles only.** Never use `common.c`'s global `env` / `conn`. A crash in one TC would poison all subsequent TCs.
2. **Always `[No crash - PASS]`.** Print regardless of SQL return — the stability criterion is "returned from the call without segfault", not "SQL succeeded".
3. **Always free handles.** Even on SQL_ERROR, `SQLFreeHandle` both DBC and ENV.
4. **One branch per TC.** If you need two branches hit, write two TCs. Keep the mapping 1:1 with the branch matrix.
5. **Tag the branch.** The comment above each `test_xxx()` must name the branch ID(s) the TC targets. This is what makes the coverage report auditable.

## Grouping in main()

Organize `main()` by test category, not by TC number:

```c
/* Basic functional tests */
test_no_braces_baseline();
test_normal_braced_value();
test_empty_braced_value();

/* Semicolon-in-value tests */
test_braced_with_semicolons();
test_braced_spanning_segments();

/* Escape-sequence tests */
test_escaped_closing_bracket();
test_multiple_escaped_brackets();

/* Boundary tests */
test_closep_plus_one_is_delp();
test_long_braced_value();

/* Error-path tests (should NOT crash, should return error) */
test_missing_closing_bracket_1();
test_invalid_char_after_bracket();
```

This makes it easier to add new TCs for the right reason later.

## Registering the binary

After writing the file, append to `test/tests`:

```
	exe/<lastexistingtest>-test \
	exe/<feature>-test
```

Preserve trailing backslash on all but the final line. The Unix Makefile and Windows nmake both read this file; any syntax error breaks both.

## Reference

See `test/src/bracket-parse-test.c` in the psqlodbc repo for a complete 20-TC example covering `drvconn.c::dconn_get_attributes()`.
