#include <stdio.h>
#include "bc.h"


/*
 *	The int left is equal to the number of columns left on the
 *	output line.
 */

static int	left = LINELEN;


/*
 *	Pstring is used to print a string onto the standard output,
 *	maintaining the number of characters left on the line in
 *	left.  `field' is the minimum number of characters to be used.
 *	Padding is zeros on the left.
 */

pstring(str, field)
register char	*str;
int		field;
{
	register int	len;

	len = strlen(str);
	if (field < len)
		field = len;
	if (left < field && field <= LINELEN) {
		printf("\\\n");
		left = LINELEN;
	}
	for (field -= len; field != 0; --field) {
		putchar('0');
		if (--left == 0) {
			left = LINELEN;
			printf("\\\n");
		}
	}
	for (; left < len; len -= left) {
		printf("%.*s\\\n", left, str);
		str += left;
		left = LINELEN;
	}
	printf("%s", str);
	left -= len;
}


/*
 *	Pnewln prints out a newline on standard output, updateing
 *	left.
 */

pnewln()
{
	putchar('\n');
	left = LINELEN;
}
