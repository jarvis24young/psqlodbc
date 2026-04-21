/*
 * ASan bug-watch for parse.c::getNextToken().
 *
 * This binary is expected to abort under AddressSanitizer while the bug
 * exists, because the literal-quote path reads tstr[-1] without checking
 * whether tstr points to the first byte of the string.
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

int
main(void)
{
	char sql[] = "'abc' tail";
	char token[64];
	char delim, quote, dquote, numeric;

	printf("=== parse.c ASan bug-watch: getNextToken leading quote underflow ===\n");
	printf("Expect ASan to report stack-buffer-underflow or invalid read while bug exists.\n");
	(void) getNextToken(0, '\0', sql, token, sizeof(token), &delim, &quote, &dquote, &numeric);
	printf("UNEXPECTED: no ASan failure observed. token='%s'\n", token);
	return 0;
}
