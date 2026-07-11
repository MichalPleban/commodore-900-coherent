

#include	"rico.h"
#include	"machine.h"
#include	"display.h"
#include	"subr.h"


char		texttab[MAXLINE];
struct scanline	scantab[YMAX];

slecompute()
{
	register int	y;
	
	for(y=0; y<MAXLINE; y++)
		texttab[y] = (ERASED | SCROLLABLE);
	for(y=0; y<YMAX; y++){
		scantab[y].sc_nword = 0;
		scantab[y].sc_off = 0;
	}
}

slcompute( li)
uint	li;
{
	uint	y,
		ylim;

	if ((li)
	and (li < MAXLINE)
	and (not (texttab[li]&SCROLLABLE))) {
		y = li * YSCROLL;
		ylim = y + YSCROLL;
		for (; y<ylim; ++y)
			scompute( y);
		texttab[li] |= SCROLLABLE;
	}
}


scompute( y)
uint	y;
{
	uint	*p,
		*ep,
		*q,
		*eq;

	if (y < YSPLIT) {
		p = (uint *)SEG0 + y*WPERSL;
		q = (uint *)SEG0 + (y-YSCROLL)*WPERSL;
	}
	else if (y-YSCROLL >= YSPLIT) {
		p = (uint *)SEG1 + (y-YSPLIT)*WPERSL;
		q = p - YSCROLL * WPERSL;
	}
	else {
		p = (uint *)SEG1 + (y - YSPLIT)*WPERSL;
		q = (uint *)SEG0 + (y-YSCROLL)*WPERSL;
	}
	ep = p + WPERSL;
	eq = q + WPERSL;
	loop {
		if (*p != *q)
			break;
		if (++p == ep) {
			scantab[y].sc_nword = 0;
			return;
		}
		++q;
	}
	scantab[y].sc_off = p - ep + WPERSL;
	while (*--ep == *--eq)
		;
	++ep;
	scantab[y].sc_nword = ep - p;
}
