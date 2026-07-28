#include "graph.h"
#define SCROLL_BUF MAX_LRBUF
SM_REGION s[SCROLL_BUF];
SM_REGION cheater[MAX_LRBUF];
RECT cl_area_rect;



extern POINT gkToLogical(), gkToGlobal();
extern RECT R_inset();
extern RECT R_Intersection();

/******************************************************************************

	Define the text cursor:		SM_DefChar()
	Draw the text cursor:		SM_DrawCurs()
	Text scroll the clipping region:SM_Scroll()

 *****************************************************************************/


void SM_DefCurs()
{
	register int	i;

	if ( (i = msgBytL0 + msgBytL1) < 0 )
		return;
	if ( (unsigned) msgData2 > DIS_WIDTH )
		return;
	gkCursor.x = msgData2;
	gkCursor.y = i;
	gkCascnt = msgBytL0;
}


void SM_DrawCurs()
{
	 register BLTSTRUCT blt;
	 POINT pt;

	if ( msgData1 )	/* request to turn cursor on */
		if ( gkFlags & WT_CURSOR_ON )
		{
#ifdef DEBUG
			peteprint("double on\n");
#endif
			return;
		}
		else	
			gkFlags |= WT_CURSOR_ON;
	else
		if ( gkFlags & WT_CURSOR_ON )
			gkFlags &= ~WT_CURSOR_ON;
		else
		{
#ifdef DEBUG
			peteprint("double off\n");
#endif
			return;
		}

	blt.src = &gkBitMap;
	blt.dst = &gkBitMap;
	
	if ( msgData1 )	/* on */
	{
		blt.dr.origin.x = gkDp.x;
		blt.dr.origin.y = gkDp.y - gkCascnt;
		gk.wn_Curpos =  gkToLogical(gkDp);
#ifdef DEBUG
		peteprint("cursor on \n");
#endif
	}
	else /* off */
	{
		pt = gkToGlobal(gk.wn_Curpos);
		blt.dr.origin.x = pt.x;
		blt.dr.origin.y = pt.y - gkCascnt;
#ifdef DEBUG
		peteprint("cursor off \n");
#endif
	}

	blt.dr.corner.x = blt.dr.origin.x + gkCursor.x;
	blt.dr.corner.y = blt.dr.origin.y + gkCursor.y;
	blt.sp = blt.dr.origin;
	blt.op = L_NDST;
	blt.pat = texture[0];
	lbitblt(&blt, 1, 1);

}


void SM_Scroll()
{
	extern int ALL_ON[];
	BLTSTRUCT blt;
	GETPTR	f;
	register int dy;
	register int i;
	int j;

	/* if the layer is fully visible, lblt can do it, otherwise */
	/* we must rely on the update method                        */

#ifdef DEBUG
peteprint("scroll\n");
#endif
	f.gfid = gkFont.fi_Id;
	f.gch = ' ';
	if ( FM_getch(&f) == -1 )
	{
		msgCmd = WM_ACK;
		sendmsg(&msg);
		return;
	}
	dy = msgData2 * f.gleading;

	if ( msgData2 == 0 )
	{
#ifdef DEBUG
		peteprint("msgData2 == 0 in scroll\n");
#endif
		dy = f.gleading;
	}

	if ( dy <= 0 )
	{
		msgCmd = WM_ACK;
		sendmsg(&msg);
		return;
	}

	if ( dy >= gkCrect.corner.y - gkCrect.origin.y )
	{
		SM_ClrClip();
		gk.wn_Flags &= ~WT_CURSOR_ON;
		msgCmd = WM_ACK;
		sendmsg(&msg);
		return;
	}

	/* turn the cursor off */
	if ( gkFlags & WT_CURSOR_ON )
	{
		i = msgData1;
		msgData1 = 0;
		SM_DrawCurs();
		gkFlags |= WT_CURSOR_WON;
		msgData1 = i;
	}

	cl_area_rect = gkCrect;
	if ( msgData1 )		/* down */
		cl_area_rect.corner.y = gkCrect.origin.y + dy;
	else			/* up  */
		cl_area_rect.origin.y = gkCrect.corner.y - dy;


	blt.src = blt.dst = &gkBitMap;
	blt.op = L_SRC;
	blt.pat = ALL_ON;
	blt.sp.x = gkCrect.origin.x;
	blt.dr = gkCrect;	/* all we really want here are the x's */

	if ( gkFullyVis )
	{
		if ( msgData1 )
		{
			/* Scroll Down */
			blt.sp.y = gkCrect.origin.y;
			blt.dr.origin.y = gkCrect.origin.y + dy;
			blt.dr.corner.y = gkCrect.corner.y;
			bitblt(&blt, 0, 1);
			blt.dr.corner.y = blt.dr.origin.y;
			blt.dr.origin.y -= dy;
	
			/* gk.wn_Curpos.y += dy; */
		}
		else
		{
			/* Scroll Up */
			blt.sp.y = gkCrect.origin.y + dy;
			blt.dr.origin.y = gkCrect.origin.y;
			blt.dr.corner.y = gkCrect.corner.y - dy;
			bitblt(&blt, 0, 1);
			blt.dr.origin.y = blt.dr.corner.y;
			blt.dr.corner.y += dy;

		/*	gk.wn_Curpos.y -= dy; */
		}
		blt.op = L_TRUE;
		blt.pat = texture[ msgBytL0 ? gkBpat : gkFpat ];
		bitblt(&blt, 0, 1);
		msgCmd = WM_ACK;
		sendmsg(&msg);
		if ( gkFlags & WT_CURSOR_WON )
		{
			msgData1 = 1;
			SM_DrawCurs();
			gkFlags &= ~WT_CURSOR_WON;
		}
		return;
	}



	/* window not fully visible... harder case to handle */
	/* save off real layer region list */
/*
	for ( i = 0; i < MAX_LRBUF; i++ )
		cheater[i] = gkLayer->reg[i];	
*/
	fcpy(cheater, gkLayer->reg, sizeof(SM_REGION)*MAX_LRBUF/2); 
	
	mk_sc_list(msgData1, dy);

	/* install our new cheat version */
/*
	for ( i = 0; i < SCROLL_BUF; i++ )
		gkLayer->reg[i] = s[i];
*/
	fcpy(gkLayer->reg, s, sizeof(SM_REGION)*SCROLL_BUF/2); 
	
	/* set the right destination rectangle and source point */
	if ( msgData1 ) /* down */
	{
		blt.sp.y = gkCrect.origin.y;
		blt.dr.origin.y = gkCrect.origin.y + dy;
		blt.dr.corner.y = gkCrect.corner.y;
		gk.wn_Curpos.y += dy;
	}
	else /* up */
	{
		blt.sp.y = gkCrect.origin.y + dy;
		blt.dr.origin.y = gkCrect.origin.y;
		blt.dr.corner.y = gkCrect.corner.y - dy;
		gk.wn_Curpos.y -= dy;
	}
	/* perform the visible portion of the scroll */
	lblt(&blt, 1, 1);
		

	/* now erase the sections which did not get stuff in'em*/
	/* replace the real layer definition */
/*
	for ( i = 0; i < MAX_LRBUF; i++ )
		gkLayer->reg[i] = cheater[i];
*/
	fcpy(gkLayer->reg, cheater, sizeof(SM_REGION)*MAX_LRBUF/2);

	mk_sc_vlist(msgData1, dy);

	/* now eliminate all non-visible regions */
	for ( i = 0; i < MAX_LRBUF && gkLayer->reg[i].flag != L_EMPTY; i++)
		if ( gkLayer->reg[i].flag == L_OBSCURED )
		{
			for ( j = i; gkLayer->reg[j+1].flag != L_EMPTY; j++)
				gkLayer->reg[j] = gkLayer->reg[j+1];
			gkLayer->reg[j].flag = L_EMPTY;
		i--;
		}
	gkLayer->reg[i].flag = L_EMPTY;

	blt.pat = texture[0];
	blt.op = L_TRUE;
	blt.dr = gkCrect;
	blt.sp = gkCrect.origin;
	lblt(&blt, 1, 1);

	/* having blanked out the areas, we now remove all bitmap regions */
	/* that intersect the clear_area_rect                             */

	for ( i = 0; i < SCROLL_BUF && gkLayer->reg[i].flag != L_EMPTY; i++ )
		if ( R_intersect(cl_area_rect, gkLayer->reg[i].bm.rect) )
		{
			for ( j = i; gkLayer->reg[j+1].flag != L_EMPTY; j++ )
				gkLayer->reg[j] = gkLayer->reg[j+1];
			gkLayer->reg[j].flag = L_EMPTY;
			i--;
		}
	gkLayer->reg[i].flag = L_EMPTY;

	/* If there is anything left, redraw the ascii map */

	if ( gkLayer->reg[0].flag != L_EMPTY )
		redraw_map(gkWid);
	/* replace the real layer definition */
/*
	for ( i = 0; i < MAX_LRBUF; i++ )
		gkLayer->reg[i] = cheater[i];
*/

	fcpy(gkLayer->reg, cheater, sizeof(SM_REGION)*MAX_LRBUF/2);

	msgCmd = WM_ACK;
	sendmsg(&msg);
	if ( gkFlags & WT_CURSOR_WON )
	{
		msgData1 = 1;
		SM_DrawCurs();
		gkFlags &= ~WT_CURSOR_WON;
	}
}


mk_sc_list(dir, yoff)
int 	dir;
int	yoff;
{
	register int i,j,k;

#ifdef DEBUG
peteprint("mk_sc_list\n");
#endif
	for ( i = 0; i < SCROLL_BUF; i++)
		s[i].flag = L_EMPTY;

	/* make a list of all rectangle , either in topright to bottomleft
	 * order for scrolling up or bottomright to topleft order for 
   	 * scrolling down .  The list, produced in the global array of 
	 * structures 's', is terminated by the element containing the
	 * flag value L_EMPTY (0).
	 */

	if ( dir == 0 )  /* up ? */
	{
	  for ( i = 0; i < MAX_LRBUF && gkLayer->reg[i].flag != L_EMPTY; i++ )
	    if ( gkLayer->reg[i].flag == L_VISIBLE )
	      for ( j = 0 ; j < SCROLL_BUF; j++ )
	        if ( s[j].flag == L_EMPTY ) 
		{
	          s[j] = gkLayer->reg[i];
		  s[j].bm.rect.corner.y -= yoff;
		  break;
		}
        	else if ( RTL(gkLayer->reg[i].bm.rect.origin,
                              s[j].bm.rect.origin) )
		{
		  /* shift all current above 'j' up one */
		  for (k = SCROLL_BUF - 1; k > j; k-- )
			s[k] = s[k - 1];
		  s[j] = gkLayer->reg[i];
		  s[j].bm.rect.corner.y -= yoff;
		  break;
		}
	}
	  else	/* downward scrolling */
	{
	    for ( i = 0; i < MAX_LRBUF && gkLayer->reg[i].flag != L_EMPTY; i++ )
	      if ( gkLayer->reg[i].flag == L_VISIBLE )
	        for ( j = 0 ; j < SCROLL_BUF; j++ )
	          if ( s[j].flag == L_EMPTY ) 
		  {
	            s[j] = gkLayer->reg[i];
		    s[j].bm.rect.origin.y += yoff;
		    break;
		  }
	          else if ( RBR(gkLayer->reg[i].bm.rect.origin, 
                                s[j].bm.rect.origin) )
		  {
		    /* shift all current above 'j' up one */
		    for (k = SCROLL_BUF - 1; k > j; k-- )
			  s[k] = s[k - 1];
		    s[j] = gkLayer->reg[i];
		    s[j].bm.rect.origin.y += yoff;
		    break;
		  }
	  }

	/* now clip all rectangle of all bitmaps to the current clipping
 	 * rectangle .  This also computes the final base address for
	 * all visible regions.
	 */
	for ( i = 0; i < SCROLL_BUF && s[i].flag != L_EMPTY; i++ )
	{
		s[i].bm.rect = R_Intersection(s[i].bm.rect, gkCrect);
		/* If the rectangle is now null, eliminate it
		 * else recompute the base address for it 
		 */
		if ( R_null(s[i].bm.rect))
		{
			for ( j = i; j < (SCROLL_BUF - 1); j++ )
				s[j] = s[j+1];
			s[SCROLL_BUF-1].flag = L_EMPTY;
			i--;  /* back 'i' up by one */
		}
		else
			s[i].bm.base = screen_addr(s[i].bm.rect.origin);
	}
				
	
}



mk_sc_vlist(dir, yoff)
int 	dir;
int	yoff;
{
	register RECT	r;
	register int i;


	/* note that the fake list is now in gkLayer->reg[*]  */
	/* This routine computes, only for the visible regions,
	 * the correct area to be cleared out which is also the
	 * area to run the ascii map through
	 */

	if ( dir == 0 )	/* scrolling up */
	{
		/* for each element of the fake list, get only the */
		/* lower yoff lines of the region                  */
		for ( i = 0; i < MAX_LRBUF && gkLayer->reg[i].flag != L_EMPTY; i++ )
		{
			if ( gkLayer->reg[i].flag == L_VISIBLE )
			{
			  r = gkLayer->reg[i].bm.rect;
			  if ( R_intersect(r, cl_area_rect) )
				r = R_Intersection(r, cl_area_rect);
			  else  
				if ( ( r.corner.y - yoff) > r.origin.y )
					r.origin.y = r.corner.y - yoff;
		    	  gkLayer->reg[i].bm.base = screen_addr(r.origin);
			  gkLayer->reg[i].bm.rect = r;
			}
		}

		gkLayer->reg[i].flag = L_EMPTY;
	}
	else	
	{
		/* for each element of the fake list, get only the */
		/* top yoff lines of the region                    */
		for ( i = 0; i < MAX_LRBUF && gkLayer->reg[i].flag != L_EMPTY; i++ )
		{
			if ( gkLayer->reg[i].flag == L_VISIBLE )
			{
			  r = gkLayer->reg[i].bm.rect;
			  if ( R_intersect(r, cl_area_rect) )
				r = R_Intersection(r, cl_area_rect);
			  else
				if ( ( r.origin.y + yoff) < r.corner.y )
					r.corner.y = r.origin.y + yoff;
			  gkLayer->reg[i].bm.rect = r;
			  gkLayer->reg[i].bm.base = screen_addr(r.origin);
			}
		}
		gkLayer->reg[i].flag = L_EMPTY;
	}
}



bool RTL(p1, p2)	/* is r1 top left of r2 */
POINT p1, p2;
	
{
	if ( p1.y < p2.y )
		return(1);
	if ( (p1.y == p2.y) && (p1.x < p2.x) )
		return(1);
	return(0);
}


bool RBR(p1, p2)	/* is r1 below right of r2 */
POINT p1, p2;
{
	if ( p1.y > p2.y)
		return(1);
	if ( (p1.y == p2.y) && (p1.x > p2.x) )
		return(1);
	return(0);
}
