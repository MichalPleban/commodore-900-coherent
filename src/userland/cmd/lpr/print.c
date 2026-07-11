

#include	<stdio.h>


#define	LINESIZ	132

extern FILE	*lp;			/* printer output (defined in lpd1.c) */

static		col,			/* current print position */
		endcol;			/* leftmost unused position */
static char	line0[LINESIZ],
		line1[LINESIZ];


print( file)
char	*file;
{
	register FILE	*f;
	register	c;
	extern		printing;

	f = fopen( file, "r");
	if (f == NULL)
		return (1);
	lputc( '\f');

	while ((c=getline( f)) && printing>0) {
		putline( line1);
		putline( line0);
		lputc( c);
	}

	fclose( f);
	return (0);
}



getline( f)
register FILE	*f;
{
	register	c;

	col = 0;
	endcol = 0;

	while ((c=getc( f)) != EOF)
		switch (c) {
		case '\n':
		case '\f':
			line0[col] = '\0';
			line1[col] = '\0';
			return (c);
		case '\b':
			if (col)
				--col;
			break;
		case '\r':
			col = 0;
			break;
		case '\t':
			do {
				store( ' ');
			} while (col & 7);
			break;
		default:
			store( c);
		}

	return (0);
}


store( c)
{

	if (col < LINESIZ-1)
		if (col >= endcol) {
			line0[col] = c;
			line1[col] = ' ';
			++endcol;
		}
		else
			line1[col] = c;
	++col;
}


putline( linep)
register char	*linep;
{
	register	c,
			xcol;

	col = 0;
	xcol = 0;

	for (; ; )
		switch (c = *linep++) {
		case '\0':
			if (col)
				lputc( '\r');
			return;
		default:
			while (col < xcol) {
				++col;
				putc( ' ', lp);
			}
			putc( c, lp);
			++col;
		case ' ':
			++xcol;
		}
}


lputc( c)
{

	putc( c, lp);
}
