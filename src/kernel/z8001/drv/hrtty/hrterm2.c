/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include	<signal.h>
#include	<errno.h>
#include	"rico.h"
#include	"machine.h"
#include	"display.h"
#include	"subr.h"
#include	"font.h"

extern	unsigned short	nulls[];
uint	line,
	col,
	iget,
	iput,
	inl;
	state;
bool	iesc;

/*
**	Screen Initialization routine.  Called by htload
*/
ricoload()
{
	register	unsigned short i,j;
	unsigned	short *sp;
/*	for(i = 0; i <YSCROLL*WPERSL;i++)nulls[i] = 0xffff;	#### */
	for(i = 0; i <YSCROLL*WPERSL;i++)nulls[i] = 0x0;
	sp = SEG0;
	for(i = 0; i<512; i++)
		for(j = 0; j < 1024/16; j++)
/*			*sp++ = 0xffff;		####	*/
			*sp++ = 0x0;
	sp = SEG1;
	for(i = 0; i< 800-512; i++)
		for(j = 0; j < 1024/16; j++)
/*			*sp++ = 0xffff;		####	*/
			*sp++ = 0x0;
	state = 0;
	col = 0;
	line = MAXLINE-1;
	cursor();
}

rico( c)
{
	static uint	
			arg,
			args[2];

	switch (state) {
	/*
	 * no ongoing escape sequence
	 */
	case 0:
		switch (c) {
		case '\33':
			state = 1;
			break;
		case '\n':
			++line;
			if (line >= MAXLINE) {
				uint i;
				for(i=0;i<YSCROLL;++i)
					scompute((MAXLINE-1)*YSCROLL+i);
				line = MAXLINE - 1;
				scroll( );
			}
			col = 0;
			break;
		case '\r':
			col = 0;
			break;
		case '\t':
			col = tab( col);
			break;
		case '\b':
			if (col)
				--col;
			break;
		case ctrl( 'G'):
			bell( );
			break;
		default:
			plotc( c, col, line);
			++col;
		}
		break;
	/*
	 * ESC
	 */
	case 1:
		switch (c) {
		case '[':
			arg = nel( args);
			do {
				args[--arg] = 0;
			} while (arg);
			state = 2;
			break;
		default:
			state = 0;
			rico( c);
			break;
		}
		break;
	/*
	 * ESC [
	 */
	case 2:
		switch (c) {
		default:
			if ((isascii( c))
			and (isdigit( c))
			and (arg < nel( args))) {
				args[arg] = args[arg]*10 + c - '0';
				break;
			}
			state = 0;
			rico( c);
			break;
		case ';':
			++arg;
			break;
		case 'H':		/* Cursor Home */
			col = 0;
			if (args[1])
				col = args[1] - 1;
			if (args[0])
				--args[0];
			if (args[0] < MAXLINE)
				line = args[0];
			state = 0;
			break;
		case 'A':		/* Cursor Up */
			if (not args[0])
				++args[0];
			if (args[0] <= line)
				line -= args[0];
			state = 0;
			break;
		case 'B':		/* Cursor Down */
			if (not args[0])
				++args[0];
			if (args[0] < MAXLINE-1)
				line += args[0];
			state = 0;
			break;
		case 'D':
			if (not args[0])
				++args[0];
			if (args[0] <= col)
				col -= args[0];
			state = 0;
			break;
		case 'C':
			if (not args[0])
				++col;
			else
				col += args[0];
			state = 0;
			break;
		case 'E':		/* Erase Screen */
			for(line = 0; line < MAXLINE; line++) 
			{
				linerase(line);
				slcompute(line);
			}
			line = 0;
			col = 0;
			state = 0;
			break;
		case 'L':
			scrolld( line);
			col = 0;
			state = 0;
			break;
		case 'M':
			scrollu( line);
			col = 0;
			state = 0;
			break;
		case 'K':
			eolerase( line, col);
			state = 0;
			break;
		case '@':
			cinsert( args[0]);
			state = 0;
			break;
		case 'P':
			cdelete( args[0]);
			state = 0;
			break;
		}
	}
}
isascii(c) char c;{ return((c >= 0x20)&&(c <= 0x7f));}
isdigit(c) char c;{ return((c >= '0')&&(c<='9'));}


scrolld( li)
uint	li;
{
	uint	y,
		ybase,
		n,
		o,
		*p,
		*q,
		co;
	int	i;
	for (n=MAXLINE-1; n>li; --n)
		if (not (texttab[n]&SCROLLABLE))
			slcompute( n);
	for (n=MAXLINE-1; n>li; --n)
	{
		i = n-1;
		texttab[n] = texttab[i];
	}
	ybase = li * YSCROLL;
	y = (MAXLINE-1) * YSCROLL;
	while (y > ybase) {
		--y;
		if (y >= YSPLIT) {
			p = (uint *)SEG1 + (y-YSPLIT)*WPERSL;
			q = p + YSCROLL*WPERSL;
		} else if (y+YSCROLL < YSPLIT) {
			p = (uint *)SEG0 + y*WPERSL;
			q = p + YSCROLL*WPERSL;
		} else {
			p = (uint *)SEG0 + y*WPERSL;
			q = (uint *)SEG1 + (y+YSCROLL-YSPLIT)*WPERSL;
		}
		n = scantab[y+YSCROLL].sc_nword;
		if (n) {
			o = scantab[y+YSCROLL].sc_off;
			p += o;
			q += o;
			aldir( q, p, n);
		}
		scantab[y+YSCROLL] = scantab[y];
	}
	eolerase( li, 0);
	if (li+1 < MAXLINE)
		texttab[li+1] &= ~SCROLLABLE;
}


eolerase( li, co)
uint	li,
	co;
{

	if (texttab[li] & ERASED)
		return;
	if (not co) {
		linerase( li);
		texttab[li] |= ERASED;
	}
	else
		for (; co<MAXCOL; ++co)
			plotc( ' ', co, li);
	texttab[li] &= ~SCROLLABLE;
	if (li+1 < MAXLINE)
		texttab[li+1] &= ~SCROLLABLE;
}


cinsert( n)
uint	n;
{
	uint	y,
		ylim,
		z,
		co;
	uchar	*p;

	if ( not n)
		n = 1;
	y = line * YSCROLL;
	ylim = y + YSCROLL;
	for (; y<ylim; ++y) {
		if (y < YSPLIT)
			p = (uchar *)SEG0 + y*BPERSL;
		else
			p = (uchar *)SEG1 + (y-YSPLIT)*BPERSL;
		if (n <= MAXCOL)
			for (co=MAXCOL-n; co>col; ) {
				--co;
				tmove(co+n, co, p);
			}
/*
		for (co=col; co<col+n && co<MAXCOL; ++co){
			z = (co * 3) >> 1;
			if( co & 1){
				p[z] |= 0x0F;
				p[z+1] |= 0xFF;
			}
			else{
				p[z] |= 0xFF;
				p[z+1] |= 0xF0;
			}
		}
*/
	}
	texttab[line] &= ~SCROLLABLE;
	if (line+1 < MAXLINE)
		texttab[line+1] &= ~SCROLLABLE;
}


cdelete( n)
uint	n;
{
	uint	y,
		ylim,
		z,
		co;
	uchar	*p;

	if (not n)
		n = 1;
	y = line * YSCROLL;
	ylim = y + YSCROLL;
	for (; y<ylim; ++y) {
		if (y < YSPLIT)
			p = (uchar *)SEG0 + y*BPERSL;
		else
			p = (uchar *)SEG1 + (y-YSPLIT)*BPERSL;
		for (co=col; co+n<MAXCOL; ++co)
			tmove(co, co+n, p);
		for (; co<MAXCOL; ++co){
			z = (co * 3) >> 1;
			if(co & 1){
				p[z] |= 0x0F;
				p[z+1] |= 0xFF;
			}
			else{
				p[z] |= 0xFF;
				p[z+1] |= 0xF0;
			}
		}
	}
	texttab[line] &= ~SCROLLABLE;
	if (line+1 < MAXLINE)
		texttab[line+1] &= ~SCROLLABLE;
}

plotc( c, co, li)
uint	co,
	li;
{
	register short	*fp;
	register char
			*p,
			b;
	register uint	y,
			ylim;

	if (co >= MAXCOL)
		return;
	if((c > 0x7f) && (c < 0xa0)) return;
	if(c < 0x20) return;
	fp = font[c-' '];
	if(c & 0x80) fp = font[c-0x40];
	y = li * YSCROLL;
	ylim = y + YSCROLL;
	if (co&1) {
		for(; y<ylim; y++) {
			if(y < YSPLIT)
				p = (char *)SEG0+y*BPERSL + (co*12)/8;
			else
				p = (char *)SEG1+(y-YSPLIT)*BPERSL+
					(co*12)/8;
/*			b = (char)((~(*fp>>12))&0xf);	*/
			b = (char)(((*fp>>12))&0xf);
			*p = (*p&0xf0)|b;
/*			*++p = ~((char)(*fp++>>4));	*/
			*++p = ((char)(*fp++>>4));
		}
	}
	else {
		
		for (; y<ylim; y++) {
			if (y < YSPLIT)
				p = (char *)SEG0 + y*BPERSL + (co*12/8);
			else
				p = (char *)SEG1 + (y-YSPLIT)*BPERSL 
					+ (co*12)/8;
/*			*p++ = (char)(~((*fp)>>8));
			*p = (*p&0xf) | (char)((~(*fp++))&0xf0);	*/
			*p++ = (char)(((*fp)>>8));
			*p = (*p&0xf) | (char)(((*fp++))&0xf0);
		}

	}
	texttab[li] &= ~ (SCROLLABLE|ERASED);
	if (line+1 < MAXLINE)
		texttab[line+1] &= ~SCROLLABLE;
}


bell( )
{
	uint	i,
		*p;

	p = SEG0;
	for (i=0; i<2*YSCROLL*WPERSL; ++i)
		*p++ ^= ~0;
	p = SEG0;
	for (i=0; i<2*YSCROLL*WPERSL; ++i)
		*p++ ^= ~0;
}


cursor( )
{
	uchar	*p;
	uint	y,
		ylim;

	if (col >= MAXCOL)
		return;
	y = line * YSCROLL;
	ylim = y + YSCROLL;
	if(col&1) {
		
		for (; y<ylim; ++y) {
			if (y < YSPLIT)
				p = (char *)SEG0 + y*BPERSL + (col*12)/8;
			else
				p = (char *)SEG1 + (y-YSPLIT)*BPERSL + 
					(col*12)/8;
			*p++ ^=0x0f;
			*p ^= 0xff;
		}
	}
	else{
		for(; y<ylim; ++y) {
			if( y < YSPLIT)
				p = (char *)SEG0 + y*BPERSL +
					(col*12)/8;
			else
				p = (char *)SEG1 + y*BPERSL +
					(col*12)/8;
			*p++ ^= 0xff;
			*p ^= 0xf0;
		}
	}
}
 
/* given dest, src col numbers will move 12 bits of a char in raster */
 
int	tmove(dst, src, p)
register int	dst;	/* destination column */
register int	src;	/* source column */
uchar	*p;	/* raster address */
{
	register	tdst,
			tsrc;
 
	tdst = (dst * 3) >> 1;	/* for 12 bits 12/8 */
	tsrc = (src * 3) >> 1;
 
	if( dst & 1){	/* odd dst */
		if( src & 1){	/* odd src */
			p[tdst] = (p[tsrc] & 0x0F) | (p[tdst] & 0xF0);
			p[tdst+1] = p[tsrc+1];
		}
		else{	/* even src */
			p[tdst] = (p[tdst] & 0xF0) | ((p[tsrc] >> 4) & 0x0F);
			p[tdst + 1] = (p[tsrc] << 4) | (p[tsrc+1] >> 4);
		}
	}
	else{		/* even dst */
		if( src & 1){ 	/* odd src */
			p[tdst] = (p[tsrc] << 4) | (p[tsrc+1] >> 4);
			p[tdst+1] = (p[tdst+1] & 0x0F) | (p[tsrc+1] << 4);
		}
		else{	/* even src */
			p[tdst] = p[tsrc];
			p[tdst+1] = (p[tdst+1] & 0x0F) | (p[tsrc+1] & 0xF0);
		}
	}
}
 
