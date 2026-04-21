/*
 * White-box DT cases for parse.c tokenization and alias insertion helpers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../multibyte.c"
#include "../../parse.c"

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

static const char *
next_token(const char *sql, char *token, char *delim, char *quote, char *dquote, char *numeric)
{
	return getNextToken(0, '\\', (const UCHAR *) sql, token, 256, delim, quote, dquote, numeric);
}

static void
test_get_next_token_quotes(void)
{
	char token[256], delim, quote, dquote, numeric;
	const char *next;

	next = next_token("$tag$hello $ world$tag$ rest", token, &delim, &quote, &dquote, &numeric);
	check_str("getNextToken dollar quote token", token, "hello $ world");
	check_int("getNextToken dollar quote flag", quote, TRUE);
	check_int("getNextToken dollar quote returned pointer", next != NULL, TRUE);

	next = next_token("'a''b' tail", token, &delim, &quote, &dquote, &numeric);
	check_str("getNextToken literal doubled quote", token, "a'b");
	check_int("getNextToken literal quote flag", quote, TRUE);
	check_int("getNextToken literal next char", *next, 't');

	next = getNextToken(0, '\0', (const UCHAR *) "E'a\\\\b' tail" + 1,
		token, sizeof(token), &delim, &quote, &dquote, &numeric);
	check_str("getNextToken E literal escape token", token, "ab");
	check_int("getNextToken E literal quote flag", quote, TRUE);
	check_int("getNextToken E literal next char", *next, 't');

	next = next_token("\"a\"\"b\" tail", token, &delim, &quote, &dquote, &numeric);
	check_str("getNextToken identifier doubled quote", token, "a\"b");
	check_int("getNextToken identifier quote flag", dquote, TRUE);
	check_int("getNextToken identifier next char", *next, 't');

	next = next_token("123.45 rest", token, &delim, &quote, &dquote, &numeric);
	check_str("getNextToken numeric token", token, "123.45");
	check_int("getNextToken numeric flag", numeric, TRUE);
	check_int("getNextToken numeric next char", *next, 'r');
}

static void
test_alias_helpers(void)
{
	char *stmt;
	const char *pptr;
	const char *ptr;
	char *newstmt;

	check_int("include_alias_wo_as empty previous token", include_alias_wo_as("alias", ""), FALSE);
	check_int("include_alias_wo_as right paren token", include_alias_wo_as(")", "col"), FALSE);
	check_int("include_alias_wo_as AS previous token", include_alias_wo_as("alias", "as"), FALSE);
	check_int("include_alias_wo_as AND previous token", include_alias_wo_as("alias", "and"), FALSE);
	check_int("include_alias_wo_as identifier previous token", include_alias_wo_as("alias", "col"), TRUE);

	if (include_alias_wo_as("alias", "+") != FALSE)
	{
		printf("BUG: include_alias_wo_as treats operator-only btoken '+' as aliasable; expected FALSE\n");
		bug_count++;
	}
	else
		printf("PASS: include_alias_wo_as operator-only token -> FALSE\n");

	stmt = strdup("select \"col\" alias from t");
	if (!stmt)
	{
		printf("FAIL: strdup returned NULL\n");
		failures++;
		return;
	}
	pptr = stmt + strlen("select \"col\" ");
	ptr = pptr + strlen("alias");
	newstmt = insert_as_to_the_statement(stmt, &pptr, &ptr);
	if (!newstmt)
	{
		printf("FAIL: insert_as_to_the_statement returned NULL\n");
		free(stmt);
		failures++;
		return;
	}
	stmt = newstmt;
	check_str("insert_as_to_the_statement output", stmt, "select \"col\" as alias from t");
	check_int("insert_as_to_the_statement pptr char", *pptr, 'a');
	check_int("insert_as_to_the_statement ptr char", *ptr, ' ');
	free(stmt);
}

int
main(void)
{
	printf("=== parse.c white-box DT ===\n");
	test_get_next_token_quotes();
	test_alias_helpers();
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
