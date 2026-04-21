#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "common.h"

static void
show_interval_text(HSTMT hstmt)
{
	SQLRETURN	rc;
	SQLLEN		ind = 0;
	char		buf[128];
	const char *sql =
		"SELECT '4 days 05:06:07.123456'::interval AS iv";

	memset(buf, 0, sizeof(buf));

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while inspecting interval text", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed while inspecting interval text", hstmt);

	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData(SQL_C_CHAR) failed while inspecting interval text", hstmt);

	printf("server interval text: %s\n", buf);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static long
expected_fraction_for_precision(SQLSMALLINT precision)
{
	char		fraction[] = "123456000";

	if (precision < 0)
		precision = 6;
	if (precision == 0)
		return 0;
	if (precision > 9)
		precision = 9;
	fraction[precision] = '\0';
	return atol(fraction);
}

static void
run_interval_case(HSTMT hstmt, SQLSMALLINT precision, long expected_fraction)
{
	SQLRETURN			rc;
	SQLHDESC			hdesc = SQL_NULL_HDESC;
	SQL_INTERVAL_STRUCT	intervalval;
	SQLLEN				ind = SQL_NULL_DATA;
	const char		   *sql =
		"SELECT '4 days 05:06:07.123456'::interval AS iv";

	memset(&intervalval, 0, sizeof(intervalval));

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescField(hdesc, 1, SQL_DESC_CONCISE_TYPE,
						 (SQLPOINTER) (intptr_t) SQL_C_INTERVAL_DAY_TO_SECOND, 0);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetDescField(SQL_DESC_CONCISE_TYPE) failed", SQL_HANDLE_DESC, hdesc);
		exit(1);
	}

	rc = SQLSetDescField(hdesc, 1, SQL_DESC_PRECISION,
						 (SQLPOINTER) (intptr_t) precision, 0);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetDescField(SQL_DESC_PRECISION) failed", SQL_HANDLE_DESC, hdesc);
		exit(1);
	}

	printf("running blackbox interval fetch with precision=%d\n", (int) precision);
	fflush(stdout);

	rc = SQLGetData(hstmt, 1, SQL_ARD_TYPE, &intervalval, sizeof(intervalval), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData(SQL_ARD_TYPE) failed", hstmt);

	printf("interval_type=%u sign=%u day=%u hour=%u minute=%u second=%u fraction=%u ind=%ld\n",
		   (unsigned int) intervalval.interval_type,
		   (unsigned int) intervalval.interval_sign,
		   (unsigned int) intervalval.intval.day_second.day,
		   (unsigned int) intervalval.intval.day_second.hour,
		   (unsigned int) intervalval.intval.day_second.minute,
		   (unsigned int) intervalval.intval.day_second.second,
		   (unsigned int) intervalval.intval.day_second.fraction,
		   (long) ind);

	if ((long) intervalval.intval.day_second.fraction != expected_fraction)
	{
		fprintf(stderr,
				"unexpected fraction for precision=%d: got=%ld expected=%ld\n",
				(int) precision,
				(long) intervalval.intval.day_second.fraction,
				expected_fraction);
		exit(1);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLSMALLINT	repro_precision = 6;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SET intervalstyle=sql_standard", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SET intervalstyle failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	show_interval_text(hstmt);
	run_interval_case(hstmt, 6, expected_fraction_for_precision(6));

	/*
	 * This is the real black-box bug-watch:
	 * SQLGetData(SQL_ARD_TYPE) -> PGAPI_GetData() -> copy_and_convert_field()
	 * -> interval2istruct() -> getPrecisionPart().
	 *
	 * Run with a larger precision argument, e.g. "20", to hit the bug from the
	 * real ODBC path. On the buggy build, that reaches fraction[precision] =
	 * '\0' in getPrecisionPart(). After fixing the bug by clamping to 9 digits,
	 * the expected value is computed by expected_fraction_for_precision().
	 */
	if (argc > 1)
	{
		repro_precision = (SQLSMALLINT) atoi(argv[1]);
		run_interval_case(hstmt, repro_precision,
						  expected_fraction_for_precision(repro_precision));
	}

	test_disconnect();
	return 0;
}
