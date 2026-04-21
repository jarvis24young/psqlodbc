/*
 * ASan bug-watch for convert.c::getPrecisionPart().
 *
 * This binary is expected to abort under AddressSanitizer while the bug
 * exists, because precision is used as an unchecked index into a 10-byte
 * local buffer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char forced_escape;

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

int
main(void)
{
	/*
	 * With the current implementation, precision > 9 writes past the end of:
	 *   char fraction[] = "000000000";
	 */
	printf("=== convert.c ASan bug-watch: getPrecisionPart overflow ===\n");
	printf("Expect ASan to report stack-buffer-overflow while bug exists.\n");
	(void) getPrecisionPart(20, "123");
	printf("UNEXPECTED: no ASan failure observed.\n");
	return 0;
}
