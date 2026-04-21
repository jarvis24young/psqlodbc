/*
 * White-box DT cases for selected dlg_specific.c configuration helpers.
 *
 * SQLWritePrivateProfileString is replaced with a deterministic stub so the
 * tests can assert exactly which INI keys would be written without depending
 * on a host ODBC registry implementation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int stub_write_calls;
static int stub_fail_after = -1;
static char stub_keys[32][64];
static char stub_values[32][128];

#define SQLWritePrivateProfileString test_SQLWritePrivateProfileString

#include "../../secure_sscanf.c"
#include "../../dlg_specific.c"

int
get_mylog(void)
{
	return -1;
}

int
get_qlog(void)
{
	return -1;
}

const char *
po_basename(const char *path)
{
	return path;
}

int
mylog(const char *fmt, ...)
{
	(void) fmt;
	return 0;
}

int
myprintf(const char *fmt, ...)
{
	(void) fmt;
	return 0;
}

int
qlog(char *fmt, ...)
{
	(void) fmt;
	return 0;
}

int
qprintf(char *fmt, ...)
{
	(void) fmt;
	return 0;
}

size_t
strncpy_null(char *dst, const char *src, ssize_t len)
{
	size_t i;

	if (NULL == dst || len <= 0)
		return 0;
	for (i = 0; src[i] && i < (size_t) (len - 1); i++)
		dst[i] = src[i];
	dst[i] = '\0';
	return src[i] ? strlen(src) : i;
}

BOOL
test_SQLWritePrivateProfileString(LPCSTR section, LPCSTR entry, LPCSTR value, LPCSTR filename)
{
	(void) section;
	(void) filename;
	if (stub_write_calls < 32)
	{
		snprintf(stub_keys[stub_write_calls], sizeof(stub_keys[stub_write_calls]), "%s", entry ? entry : "");
		snprintf(stub_values[stub_write_calls], sizeof(stub_values[stub_write_calls]), "%s", value ? value : "");
	}
	stub_write_calls++;
	if (stub_fail_after >= 0 && stub_write_calls > stub_fail_after)
		return FALSE;
	return TRUE;
}

static int failures = 0;
static int bug_count = 0;

static void
check_int(const char *label, long got, long expected)
{
	if (got != expected)
	{
		printf("FAIL: %s got=%ld expected=%ld\n", label, got, expected);
		failures++;
	}
	else
		printf("PASS: %s -> %ld\n", label, got);
}

static void
check_str(const char *label, const char *got, const char *expected)
{
	if (strcmp(got, expected) != 0)
	{
		printf("FAIL: %s got='%s' expected='%s'\n", label, got, expected);
		failures++;
	}
	else
		printf("PASS: %s -> '%s'\n", label, got);
}

static void
reset_stub(void)
{
	int i;

	stub_write_calls = 0;
	stub_fail_after = -1;
	for (i = 0; i < 32; i++)
	{
		stub_keys[i][0] = '\0';
		stub_values[i][0] = '\0';
	}
}

static void
fill_globals(GLOBAL_VALUES *gv)
{
	memset(gv, 0, sizeof(*gv));
	gv->commlog = 1;
	gv->debug = 2;
	gv->fetch_max = 99;
	gv->unique_index = 1;
	gv->use_declarefetch = 1;
	gv->unknown_sizes = UNKNOWNS_AS_DONTKNOW;
	gv->text_as_longvarchar = 1;
	gv->unknowns_as_longvarchar = 0;
	gv->bools_as_char = 1;
	gv->parse = 1;
	gv->max_varchar_size = 123;
	gv->max_longvarchar_size = 456;
	STRCPY_FIXED(gv->extra_systable_prefixes, "pg_toast_,pg_temp_");
}

static void
test_unfold_cx_attribute(void)
{
	ConnInfo ci;
	UInt4 flag;
	char cx[32];

	memset(&ci, 0, sizeof(ci));
	unfoldCXAttribute(&ci, "3");
	check_int("unfoldCXAttribute short count allow_keyset", ci.allow_keyset, 1);
	check_int("unfoldCXAttribute short count lf_conversion", ci.lf_conversion, 1);
	check_int("unfoldCXAttribute count<4 leaves unique_index", ci.drivers.unique_index, 0);

	memset(&ci, 0, sizeof(ci));
	flag = BIT_UPDATABLECURSORS | BIT_LFCONVERSION | BIT_UNIQUEINDEX |
		BIT_UNKNOWN_DONTKNOW | BIT_COMMLOG | BIT_DEBUG | BIT_PARSE |
		BIT_USEDECLAREFETCH | BIT_READONLY | BIT_TEXTASLONGVARCHAR |
		BIT_UNKNOWNSASLONGVARCHAR | BIT_BOOLSASCHAR | BIT_ROWVERSIONING |
		BIT_SHOWSYSTEMTABLES | BIT_SHOWOIDCOLUMN | BIT_FAKEOIDINDEX |
		BIT_TRUEISMINUS1 | BIT_BYTEAASLONGVARBINARY | BIT_USESERVERSIDEPREPARE |
		BIT_LOWERCASEIDENTIFIER | BIT_OPTIONALERRORS | BIT_FETCHREFCURSORS;
	snprintf(cx, sizeof(cx), "04%lx", (unsigned long) flag);
	unfoldCXAttribute(&ci, cx);
	check_int("unfoldCXAttribute long count unique_index", ci.drivers.unique_index, 1);
	check_int("unfoldCXAttribute unknown dontknow", ci.drivers.unknown_sizes, UNKNOWNS_AS_DONTKNOW);
	check_str("unfoldCXAttribute readonly", ci.onlyread, "1");
	check_str("unfoldCXAttribute row_versioning", ci.row_versioning, "1");
	check_int("unfoldCXAttribute fetch_refcursors", ci.fetch_refcursors, 1);

	memset(&ci, 0, sizeof(ci));
	snprintf(cx, sizeof(cx), "04%lx", (unsigned long) BIT_UNKNOWN_ASMAX);
	unfoldCXAttribute(&ci, cx);
	check_int("unfoldCXAttribute unknown as max", ci.drivers.unknown_sizes, UNKNOWNS_AS_MAX);

	memset(&ci, 0, sizeof(ci));
	unfoldCXAttribute(&ci, "040");
	check_int("unfoldCXAttribute unknown longest default", ci.drivers.unknown_sizes, UNKNOWNS_AS_LONGEST);
}

static void
test_write_ci_drivers(void)
{
	GLOBAL_VALUES gv;

	fill_globals(&gv);
	reset_stub();
	check_int("write_Ci_Drivers skips ODBCINST_INI", write_Ci_Drivers(ODBCINST_INI, NULL, &gv), 0);
	check_int("write_Ci_Drivers skips ODBCINST_INI calls", stub_write_calls, 0);

	reset_stub();
	check_int("writeDriversDefaults delegates to ODBCINST_INI skip", writeDriversDefaults("PostgreSQL Unicode", &gv), 0);
	check_int("writeDriversDefaults write calls", stub_write_calls, 0);

	reset_stub();
	check_int("write_Ci_Drivers writes custom file", write_Ci_Drivers("whitebox.ini", "DriverX", &gv), 0);
	check_int("write_Ci_Drivers write count", stub_write_calls, 13);
	check_str("write_Ci_Drivers first key", stub_keys[0], INI_COMMLOG);
	check_str("write_Ci_Drivers first value", stub_values[0], "1");
	check_str("write_Ci_Drivers extra prefixes key", stub_keys[12], INI_EXTRASYSTABLEPREFIXES);
	check_str("write_Ci_Drivers extra prefixes value", stub_values[12], "pg_toast_,pg_temp_");

	reset_stub();
	stub_fail_after = 10;
	check_int("write_Ci_Drivers reports stub failures", write_Ci_Drivers("whitebox.ini", "DriverX", &gv), -3);
}

static void
test_copy_conn_attributes_bug_watch(void)
{
	ConnInfo ci;
	char protocol[] = "7.4-1";

	memset(&ci, 0, sizeof(ci));
	if (!copyConnAttributes(&ci, INI_PROTOCOL, protocol))
	{
		printf("FAIL: copyConnAttributes protocol branch was not taken\n");
		failures++;
		return;
	}
	check_int("copyConnAttributes rollback_on_error", ci.rollback_on_error, 1);
	if (strcmp(protocol, "7.4") == 0)
	{
		printf("BUG: copyConnAttributes mutated caller input buffer from '7.4-1' to '%s'\n", protocol);
		bug_count++;
	}
	else
	{
		printf("FAIL: expected protocol input mutation bug was not observed, got '%s'\n", protocol);
		failures++;
	}
}

int
main(void)
{
	printf("=== dlg_specific.c white-box DT ===\n");
	test_unfold_cx_attribute();
	test_write_ci_drivers();
	test_copy_conn_attributes_bug_watch();
	if (bug_count)
		printf("=== BUG diagnostics: %d issue(s) observed without crashing ===\n", bug_count);
	if (failures)
	{
		printf("=== FAIL: %d assertion(s) failed ===\n", failures);
		return 1;
	}
	printf("=== All tests completed without crash ===\n");
	return 0;
}
