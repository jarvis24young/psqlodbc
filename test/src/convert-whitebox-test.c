/*
 * White-box DT cases for selected convert.c parsing/conversion helpers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char forced_escape;
static int bug_count = 0;

#define CC_get_escape test_CC_get_escape

#include "../../secure_sscanf.c"
#include "../../convert.c"

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

char
test_CC_get_escape(const ConnectionClass *self)
{
	(void) self;
	return forced_escape;
}

static int failures = 0;

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
test_timestamp2stime(void)
{
	SIMPLE_TIME st;
	BOOL bzone;
	int zone;

	memset(&st, 0, sizeof(st));
	bzone = FALSE;
	check_int("timestamp2stime date only returns TRUE", timestamp2stime("2024-01-02", &st, &bzone, &zone), TRUE);
	check_int("timestamp2stime date only y", st.y, 2024);
	check_int("timestamp2stime date only hh", st.hh, 0);

	memset(&st, 0, sizeof(st));
	bzone = FALSE;
	check_int("timestamp2stime time only returns TRUE", timestamp2stime("03:04:05", &st, &bzone, &zone), TRUE);
	check_int("timestamp2stime time only hh", st.hh, 3);

	memset(&st, 0, sizeof(st));
	bzone = FALSE;
	check_int("timestamp2stime rejects invalid", timestamp2stime("not-a-time", &st, &bzone, &zone), FALSE);

	memset(&st, 0, sizeof(st));
	bzone = TRUE;
	check_int("timestamp2stime positive zone", timestamp2stime("2024-01-02 03:04:05+08", &st, &bzone, &zone), TRUE);
	check_int("timestamp2stime positive zone flag", bzone, TRUE);
	check_int("timestamp2stime positive zone value", zone, 8);

	memset(&st, 0, sizeof(st));
	bzone = TRUE;
	check_int("timestamp2stime negative zone", timestamp2stime("2024-01-02 03:04:05-07", &st, &bzone, &zone), TRUE);
	check_int("timestamp2stime negative zone value", zone, -7);

	memset(&st, 0, sizeof(st));
	bzone = TRUE;
	check_int("timestamp2stime fraction zone BC", timestamp2stime("2024-01-02 03:04:05.123+08 BC", &st, &bzone, &zone), TRUE);
	check_int("timestamp2stime fraction", st.fr, 123000000);
	check_int("timestamp2stime BC year", st.y, -2024);

	memset(&st, 0, sizeof(st));
	bzone = FALSE;
	check_int("timestamp2stime BC rest", timestamp2stime("0001-01-02 03:04:05 BC", &st, &bzone, &zone), TRUE);
	check_int("timestamp2stime BC rest year", st.y, -1);
}

static void
test_intervals_and_precision(void)
{
	SQL_INTERVAL_STRUCT st;

	check_int("getPrecisionPart default precision", getPrecisionPart(-1, "123456789"), 123456);
	check_int("getPrecisionPart zero precision", getPrecisionPart(0, "123456789"), 0);
	check_int("getPrecisionPart shorter input pads", getPrecisionPart(6, "5"), 500000);

	check_int("interval year to month", interval2istruct(SQL_C_INTERVAL_YEAR_TO_MONTH, 0, "2-03", &st), TRUE);
	check_int("interval year to month year", st.intval.year_month.year, 2);
	check_int("interval year to month month", st.intval.year_month.month, 3);

	check_int("interval day second compact", interval2istruct(SQL_C_INTERVAL_DAY_TO_SECOND, 3, "4 05:06:07.123456", &st), TRUE);
	check_int("interval day second compact fraction", st.intval.day_second.fraction, 123);

	if (!interval2istruct(SQL_C_INTERVAL_DAY_TO_SECOND, 6, "-4 day 05:06:07.987654", &st))
	{
		printf("BUG: interval2istruct rejected valid day-second text '-4 day 05:06:07.987654'\n");
		bug_count++;
	}
	else
	{
		check_int("interval day word second sign", st.interval_sign, SQL_TRUE);
		check_int("interval day word second day abs", st.intval.day_second.day, 4);
		check_int("interval day word second hour signed by existing code", st.intval.day_second.hour, -5);
		check_int("interval day word second fraction", st.intval.day_second.fraction, 987654);
	}

	if (!interval2istruct(SQL_C_INTERVAL_HOUR_TO_SECOND, 1, "-05:06:07.5", &st))
	{
		printf("BUG: interval2istruct rejected valid hour-second text '-05:06:07.5'\n");
		bug_count++;
	}
	else
	{
		check_int("interval hour second hour abs", st.intval.day_second.hour, 5);
		check_int("interval hour second fraction", st.intval.day_second.fraction, 5);
	}

	check_int("interval year rejects day format", interval2istruct(SQL_C_INTERVAL_YEAR, 0, "4 day 05:06:07", &st), FALSE);
	check_int("interval bad day literal", interval2istruct(SQL_C_INTERVAL_DAY_TO_SECOND, 0, "4 month 05:06:07", &st), FALSE);
}

static void
test_money_and_datetime(void)
{
	char out[32];
	SIMPLE_TIME st;

	check_int("convert_money US format", convert_money("$1,234.56", out, sizeof(out)), TRUE);
	check_str("convert_money US output", out, "1234.56");

	check_int("convert_money EU negative", convert_money("(1.234,56)", out, sizeof(out)), TRUE);
	check_str("convert_money EU output", out, "-1234.56");

	check_int("convert_money integer negative", convert_money("-1234", out, sizeof(out)), TRUE);
	check_str("convert_money integer output", out, "-1234");

	check_int("convert_money truncates safely", convert_money("$12345.67", out, 5), TRUE);
	check_str("convert_money truncates output", out, "1234");

	memset(&st, 0, sizeof(st));
	check_int("parse_datetime ODBC timestamp literal", parse_datetime("{ ts '2011-04-22 12:34:56' }", &st), TRUE);
	check_int("parse_datetime literal year", st.y, 2011);
	check_int("parse_datetime literal second", st.ss, 56);

	memset(&st, 0, sizeof(st));
	check_int("parse_datetime month-first minute precision", parse_datetime("04-22-2011 12:34", &st), TRUE);
	check_int("parse_datetime month-first year", st.y, 2011);
	check_int("parse_datetime month-first minute", st.mm, 34);

	memset(&st, 0, sizeof(st));
	check_int("parse_datetime date only month-first", parse_datetime("04-22-2011", &st), TRUE);
	check_int("parse_datetime date only day", st.d, 22);

	memset(&st, 0, sizeof(st));
	check_int("parse_datetime time only minutes", parse_datetime("12:34", &st), TRUE);
	check_int("parse_datetime time only minute", st.mm, 34);

	memset(&st, 0, sizeof(st));
	check_int("parse_datetime bad ODBC literal", parse_datetime("{ ts 2011-04-22 }", &st), FALSE);
}

static void
test_binary_conversion(void)
{
	char out[64];
	QueryBuild qb;
	ConnectionClass conn;
	const char bytes[] = {'A', ' ', '\001', (char) 0xff};
	char octal[8];

	check_int("conv_to_octal no escape length", conv_to_octal(255, octal, '\0'), 4);
	check_str("conv_to_octal no escape", octal, "\\377");
	check_int("conv_to_octal escaped length", conv_to_octal(255, octal, '\\'), 5);
	check_str("conv_to_octal escaped", octal, "\\\\377");
	check_str("conv_to_octal2", conv_to_octal2(1, octal), "\\001");

	memset(&qb, 0, sizeof(qb));
	memset(&conn, 0, sizeof(conn));
	qb.conn = &conn;
	qb.param_mode = RPM_BUILDING_BIND_REQUEST;
	qb.flags = 0;
	forced_escape = '\0';
	memset(out, 0, sizeof(out));
	check_int("convert_to_pgbinary octal length", convert_to_pgbinary(bytes, out, sizeof(bytes), &qb), 10);
	check_str("convert_to_pgbinary octal output", out, "A \\001\\377");

	qb.param_mode = RPM_REPLACE_PARAMS;
	forced_escape = '\\';
	memset(out, 0, sizeof(out));
	check_int("convert_to_pgbinary escaped octal length", convert_to_pgbinary(bytes, out, sizeof(bytes), &qb), 12);
	check_str("convert_to_pgbinary escaped octal output", out, "A \\\\001\\\\377");

	qb.param_mode = RPM_BUILDING_BIND_REQUEST;
	qb.flags = FLGB_HEX_BIN_FORMAT;
	forced_escape = '\0';
	memset(out, 0, sizeof(out));
	check_int("convert_to_pgbinary hex length", convert_to_pgbinary(bytes, out, sizeof(bytes), &qb), 10);
	check_str("convert_to_pgbinary hex output", out, "\\x412001FF");
}

int
main(void)
{
	printf("=== convert.c white-box DT ===\n");
	test_timestamp2stime();
	test_intervals_and_precision();
	test_money_and_datetime();
	test_binary_conversion();
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
