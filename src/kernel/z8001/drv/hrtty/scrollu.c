

#include	"rico.h"
#include	"machine.h"
#include	"display.h"
#include	"subr.h"


scrollu( li)
uint	li;
{
	register uint	y,
			ylim,
			n;
	uint	o,
		*p,
		*q,
		co;
 
	for (n=li; n<MAXLINE; ++n)
			slcompute( n);
 
	for (n=(li+1); n<MAXLINE; ++n)
	{
		texttab[n-1] = texttab[n];
	}
	y = (li+1) * YSCROLL;
	ylim = MAXLINE * YSCROLL;
	for (; y<ylim; ++y) {
		n = scantab[y].sc_nword;
		if (n) {
			if (y < YSPLIT) {
				p = (uint *)SEG0 + y*WPERSL;
				q = p - YSCROLL*WPERSL;
			} else if (y-YSCROLL >= YSPLIT) {
				p = (uint *)SEG1 + (y-YSPLIT)*WPERSL;
				q = p - YSCROLL*WPERSL;
			} else {
				p = (uint *)SEG1 + (y-YSPLIT)*WPERSL;
				q = (uint *)SEG0 + (y-YSCROLL)*WPERSL;
			}
			o = scantab[y].sc_off;
			p += o;
			q += o;
			aldir( q, p, n);
		}
		scantab[y-YSCROLL] = scantab[y];
	}
	slcompute(li);
/*
	eolerase( MAXLINE-1, 0);
*/
	linerase(MAXLINE-1);
	texttab[MAXLINE-1] &= ~SCROLLABLE;
}
