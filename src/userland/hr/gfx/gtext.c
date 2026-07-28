#include "graph.h"
extern	int	ALL_ON[];


/*****************************************************************************

	NOTE... THIS FILE MODIFIED... NO LONGER HANDLES ANY 
			FONT TRANSFORMATIONS   


   Get character width -	SM_CharW()
   Get string width -		SM_StrW()
   Draw character -		SM_Char()
   Draw string -		SM_Str()
   Draw icon (character mask) -	SM_Icon()

   Entry:	msgBytL0,	a character value or string length
		msgPtr,		^string

   Notes:	The character or string is drawn or has its length calculated
		using the window's current font information.  This means those
		font face, font style, and font scaling factors are used.

		The icon or icon mask is drawn using the text scaling factors.
		I.e., scaling to double the character height causes the icon
		to also be doubled in height.  Font face and font style, how-
		ever, have no influence on the icon output.  Since all icons
		and icon masks are the same width, txIconW() simply returns
		the width of any icon or icon mask scaled using the text scal-
		ing.

		Characters and icons use a drawing verb similar to the drawing
		modes of shapes.  The difference is that framing modes are
		ignored.
******************************************************************************/

void SM_CharW()
{
	msgData1 = txStrWidth(&msgBytL0, 1);
	msgCmd = WM_REPLY;
	sendmsg(&msg);
}


void SM_Char()
{
	txStr(&msgBytL0, 1);
	msgCmd = WM_ACK;
	sendmsg(&msg);
}


void SM_StrW()
{
	char	*s;
	int	len;

	if ( !(len = msgBytL0 & 0xff) )
		len = 0x100;
	if ( (s = malloc(len)) != NULL )
	{
		getdata(msgSender, len, msgPtr, s);
		msgData1 = txStrWidth(s, len);
	}
	else
		msgData1 = 0;
	free(s);
	msgCmd = WM_REPLY;
	sendmsg(&msg);
}


void SM_Str()
{
	char	this[128];
	char	*s;
	int	len;

	s = this;
	for ( len = 0; len < 128; len++ )
		*s++ = (char) 0x0;

	if ( !(len = msgBytL0 & 0xff) )
		len = 0x100;
	getdata(msgSender, len, msgPtr, this);
	txStr(this, len);

	msgCmd = WM_ACK;
	sendmsg(&msg);
}


int txStrWidth(s, slen)
char *s;
int  slen;
{
	register int	len;
	register int	width;
	GETPTR	f;

	len = 0;
	f.gfid = gkFont.fi_Id;
	while ( slen-- > 0 )
	{
		f.gch = *s++;
		if ( (width = FM_getch(&f)) == -1 )
			return 0;
		if ( width > 0 )
			len += width;
	}
	/* scaleit */
	return (len);
}


txStr(s, slen)
char *s;
register int   slen;
{
	int	width;
	GETPTR	f;
	register BLTSTRUCT blt;
	RECT	rect;
	extern  int lblt(), bitblt();
	register int	(*output_op)();
	int	 clip;
	

#ifdef DEBUG
peteprint(" %d - %s\n", slen, s);
#endif
	if ( gkFullyVis )
	{
		output_op = bitblt;
		clip = 0;
	}
	else
	{
		output_op = lblt;
		clip = 1;
	}
		

	if ( gkFont.fi_Style != 0 )
	{
		peteprint("Font Style = %d\n", gkFont.fi_Style);
		return;
	}


	f.gch = ' ';
	f.gfid = gkFont.fi_Id;
	if ( (width = FM_getch(&f)) == -1 )
		return;
	blt.src = &f.gbmap;
	blt.dst = (BITMAP *) &gkBitMap;
	blt.op = L_SRC;

	blt.pat = texture[gkBpat];
	while ( slen-- > 0 )
	{
		f.gch = *s++;
		if ( (width = FM_getch(&f)) == -1 )
			return;

		blt.dr.origin = gkDp;
		gkDp.x += width;
		blt.dr.origin.x -= f.gkern;
		blt.dr.corner.x = blt.dr.origin.x + f.gawdth;
		blt.dr.origin.y -= f.gascent;
		blt.dr.corner.y = blt.dr.origin.y + f.gascent + f.gdescent;
		blt.dr = R_Intersection(blt.dr, gkCrect);
		blt.sp = f.gpoint;
		(*output_op)(&blt, clip, 1);
	}
	return;
}


SM_AsciiMap()
{
	
	redraw_map(gkWid);
	msgCmd = WM_ACK;
	sendmsg( &msg );
	return;
}






redraw_map(wid)
int 	wid;
{

	register int	width;
	register int	*character;
	register int 	col;
	int 	row;
	GETPTR	f;
	BLTSTRUCT blt;
	int	map[2000];
	int	startx;
	POINT dp;
	int old_wid;		/* current gkWid */
	SM_REGION reg;
	register int i;
	RECT	row_test;

	old_wid = -1;
	if ( wid != gkWid )
		if ( ( legalwindow(gkWid)) && (wtbl[gkWid] != (WSTRUCT *)NULL))
		{
			old_wid = gkWid;
			*wtbl[gkWid] = gk;
			gk = *wtbl[wid];
		}
			
	if ( gk.wn_ascii == (char*)NULL)
	{
		peteprint("attempted to fetch a null ascii map\n");
		return;
	}

	getdata( gkWmgr, sizeof(map), gk.wn_ascii, map);
	

	f.gch = ' ';
	f.gfid = gk.wn_G.wn_Font.fi_Id;
	if ( (width = FM_getch(&f)) == -1 )
		return;
	blt.src = &f.gbmap;
	blt.dst = gkLayer;
	blt.op = L_SRC;
	blt.pat = texture[0];

	dp = gkCrect.origin;
	dp.y += f.gascent;

	row_test = gkCrect;
	row_test.corner.y = row_test.origin.y + f.gleading;

	startx = dp.x;
	
	if ( gkFullyVis )
	{
		for ( row = 0; row < 25; row++ )
		{
		character = &map[row*80];
		for ( col = 0; col < 80; col++ )
		  {
		  blt.dr.origin.y = dp.y - f.gascent;
		  blt.dr.corner.y = dp.y + f.gdescent;
	
		  f.gch = (char) (*character++ & 0x007F);
		  if ( f.gch != ' ' )
		    { 
			    FM_getch(&f);
			    blt.dr.origin.x = dp.x - f.gkern;
			    blt.dr.corner.x = blt.dr.origin.x + f.gawdth;
			    blt.sp = f.gpoint;
			    bitblt(&blt, 0, 1);
		    }
		  dp.x += width;
		  }
		dp.y += f.gleading;
		dp.x = startx;
		}
	}
	else	/* gk not fully visible */
	{
		for ( row = 0; row < 25; row++ )
		{
		   for (i=0; i<MAX_LRBUF && gkLayer->reg[i].flag!=L_EMPTY; i++ )
			if ( R_intersect(row_test, gkLayer->reg[i].bm.rect) )
				goto row_involved;
		   goto inc_to_next_row;
row_involved:
		   character = &map[row*80];
		   for ( col = 0; col < 80; col++ )
		     {
		     blt.dr.origin.y = dp.y - f.gascent;
		     blt.dr.corner.y = dp.y + f.gdescent;
	
		     f.gch = (char) (*character++ & 0x007F);
		     if ( f.gch != ' ' )
		       { 
			    FM_getch(&f);
			    blt.dr.origin.x = dp.x - f.gkern;
			    blt.dr.corner.x = blt.dr.origin.x + f.gawdth;
			    blt.sp = f.gpoint;
			    blt.dr = R_Intersection(blt.dr, gkCrect);
			    for ( i = 0; i < MAX_LRBUF; i++ )
			      {
			        reg = gkLayer->reg[i];
			        if ( reg.flag == L_EMPTY)
				  break;
			        if ( reg.flag == L_VISIBLE )
			          if (R_intersect(reg.bm.rect, blt.dr) )
				    lblt(&blt, 1, 1);
			      }
		      }
		    dp.x += width;
		    }
inc_to_next_row:
		dp.y += f.gleading;
		dp.x = startx;
		row_test.origin.y += f.gleading;
		row_test.corner.y += f.gleading;
		}

	}

	if ( old_wid != -1 )
		gk = *wtbl[old_wid];
	return;
}
