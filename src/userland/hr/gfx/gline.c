#include "graph.h"

#define	BPW	16			/* bits per word */
/* mouse suppression divorced from the driver (GUI.md sec 6.2): the direct
 * ioctl(myfd,CIOMSE*) calls became gfx_cursor_hide()/gfx_cursor_show(). */

/*
 * gkLine(x, y, xf, yf, pat, logop, width)
 *
 *	Draw a straight line from (x,y) to (xf,yf) using the parameters:
 *		logop:	logical operation
 *		pat:	pattern number
 *		width:	pen width
 *		height:	pen height
 *
 *	Return:	nothing
 */
void	gkLine();

/*
 * Local functions.
 */
void	lnvector(),	/* single line vector: calls lnxfast or lnyfast	*/
	lnxfast(),	/* single line: dx > 0, dx > abs(dy)		*/
	lnyfast();	/* single line: dy > 0, dy > abs(dx)		*/

/*
 * Logical operation functions: src logop dst
 */
extern uint	_lfalse(),
		_land(),
		_landn(),
		_lsrc(),
		_lnand(),
		_ldst(),
		_lxor(),
		_lor(),
		_lnandn(),
		_lnxor(),
		_lndst(),
		_lorn(),
		_lnsrc(),
		_lnor(),
		_lnorn(),
		_ltrue();


static	RECT	lnBox;		/* vis rect ANDed with gkClip	*/
static	int	lnRn,		/* #of clipping rects in lnRect	*/
		lnPat;		/* pattern number		*/

static	uint	(*lnFtn)(),		/* ^current logical function	*/
		(*lnFtnTbl[L_BLTMAX])() =
		{
			_lfalse,  _land,  _landn, _lsrc,
			_lnand,	  _ldst,  _lxor,  _lor,
			_lnandn,  _lnxor, _lndst, _lorn,
			_lnsrc,	  _lnor,  _lnorn, _ltrue
		};


void
gkLine(x, y, xf, yf, pat, logop)
register int	x,
		y,
		xf,
		yf;
int		pat,
		logop;
{
	register SM_REGION *sp;
	int	mseoff;

	if ( (lnPat = pat) >= MAXPAT )
		return;
	if ( logop >= L_BLTMAX )
		return;
	lnFtn = lnFtnTbl[logop];
	if ( mseoff = (logop != L_NDST && logop != L_XOR && logop != L_NXOR) )
		gfx_cursor_hide();
	for ( sp = gk.wn_Layer->reg; sp < gk.wn_Layer->reg+MAX_LRBUF; sp++ )
	{
		if ( sp->flag == L_EMPTY )
			break;
		if ( sp->flag == L_VISIBLE )
		{
			lnBox = R_Intersection(sp->bm.rect, gkCrect);
			if ( !R_null(lnBox) )
				lnvector(x, y, xf, yf);
		}
	}
	if ( mseoff )
		gfx_cursor_show();
}


/*
 *	Clips the endpoints of the vector to the boundaries of lnBox.
 *	Then does the line.
 */
void
lnvector(x, y, xf, yf)
register int	x,
		y,
		xf,
		yf;
{
	long		e;
	register int	dx,
			dy;

	dx = xf - x;
	dy = yf - y;
	e = (long)yf*(long)x - (long)y*(long)xf;
	if (dy >= 0)
	{
		if ( (y >= lnBox.corner.y) || (yf < lnBox.origin.y) )
			return;
		if ( y < lnBox.origin.y )
			x = ((long)(y = lnBox.origin.y)*(long)dx + e) / dy;
		if ( yf >= lnBox.corner.y )
			xf = ((long)(yf = lnBox.corner.y-1)*(long)dx + e) / dy;
	}
	else
	{
		if ( (yf >= lnBox.corner.y) || (y < lnBox.origin.y) )
			return;
		if ( yf < lnBox.origin.y )
			xf = ((long)(yf = lnBox.origin.y)*(long)dx + e) / dy;
		if ( y >= lnBox.corner.y )
			x = ((long)(y = lnBox.corner.y-1)*(long)dx + e) / dy;
	}

	if (dx > 0)
	{
		if ( (x >= lnBox.corner.x) || (xf < lnBox.origin.x) )
			return;
		if ( x < lnBox.origin.x )
			y = ((long)(x = lnBox.origin.x)*(long)dy - e) / dx;
		if ( xf >= lnBox.corner.x )
			yf = ((long)(xf = lnBox.corner.x-1)*(long)dy - e) / dx;
		if ( dy > 0 )
			if ( dx > dy )
				lnxfast(x, y, xf, yf);
			else
				lnyfast(x, y, xf, yf);
		else
			if ( dx > -dy )
				lnxfast(x, y, xf, yf);
			else
				lnyfast(xf, yf, x, y);
	}
	else
	{
		if ( (xf >= lnBox.corner.x) || (x < lnBox.origin.x) )
			return;
		if ( xf < lnBox.origin.x )
			yf = ((long)(xf = lnBox.origin.x)*(long)dy - e) / dx;
		if ( x >= lnBox.corner.x )
			y = ((long)(x = lnBox.corner.x-1)*(long)dy - e) / dx;
		if ( dy > 0 )
			if ( -dx > dy )
				lnxfast(xf, yf, x, y);
			else
				lnyfast(x, y, xf, yf);
		else
			if ( -dx > -dy )
				lnxfast(xf, yf, x, y);
			else
				lnyfast(xf, yf, x, y);
	}
}

	
void
lnxfast(x, y, xf, yf)
register int	x,
		y;
int		xf,
		yf;
{
	register uint	*vp;
	register int	rem;
	register int	dx,
			dy,
			sy;
	int		*pat;
	uint		bit;
	extern short	*xxx();

	dx = xf - x;
	dy = yf - y;
	sy = 1;
	if (dy < 0)
	{
		dy = -dy;
		sy = -1;
	}
	rem = dx/2;
	vp = xxx(x, y, &bit);
	pat = gkTexture(lnPat) + (y & 0xf);
	loop {
		*vp = (*lnFtn)(*pat, *vp, bit);
		if ( ++x > xf )
			break;
		bit >>= 1;
		if (bit == 0)
		{
			bit = 0x8000;
			++vp;
		}
		if ( (rem -= dy) < 0 )
		{
			rem += dx;
			y += sy;
			if ( sy > 0 )
			{
				vp = addptr(vp, (ulong)2*XMAX/BPW);
				if ( y & 0xf )
					pat++;
				else
					pat -= 15;
			}
			else
			{
				vp = subptr(vp, (ulong)2*XMAX/BPW);
				if ( (y & 0xf) == 0xf )
					pat += 15;
				else
					pat--;
			}
		}
	}
}


void
lnyfast(x, y, xf, yf)
register int	x,
		y;
int		xf,
		yf;
{
	register uint	*vp;
	register int	rem;
	register int	dx,
			dy,
			sx;
	int		*pat;
	uint		bit;

	dx = xf - x;
	dy = yf - y;
	sx = 1;
	if ( dx < 0)
	{
		dx = -dx;
		sx = -1;
	}
	rem = dy/2;
	vp = xxx(x, y, &bit);
	pat = gkTexture(lnPat) + (y & 0xf);
	loop {
		*vp = (*lnFtn)(*pat, *vp, bit);
		if ( ++y > yf )
			break;
		if ( y & 0xf )
			pat++;
		else
			pat -= 15;
		vp = addptr(vp, (ulong) 2*XMAX/BPW);
		if ( (rem -= dx) < 0 )
		{
			rem += dy;
			x += sx;
			if ( sx > 0)
			{
				bit >>= 1;
				if (bit == 0)
				{
					bit = 0x8000;
					++vp;
				}
			}
			else
			{
				bit <<= 1;
				if (bit == 0)
				{
					bit = 1;
					--vp;
				}
			}
		}
	}
}


/*
 *	Calculates the word address of the pixel and sets the bit of
 *	the word mask 'bit' of the pixel of interest.
 */
static short *
xxx(x, y, bit)
register uint	x,
		y;
uint	*bit;
{
	int	 *ip;

	*bit = (uint)0x8000 >> (x%BPW);
	return addptr(SEG0, (ulong)y*(ulong)(sizeof(int)*XMAX/BPW) + sizeof(int) * x/BPW);
}
