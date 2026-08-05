#include <stdio.h>
#include "smgr.h"

extern int memfail();

extern char *malloc();
extern BITMAP display;
extern UPDATE update[];
extern RECT gfx_uprect;		/* damage rect published with each WM_UPDATE */
extern RECT R_addp(), R_subp(), R_Intersection();
extern int *screen_addr();
extern int ALL_ON[];

#define FILL_CLR	(0)
#define FILL_BCK	(1)
#define FILL_HLF	(2)



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*                                                                           */
/* lblt - layer blt call                                                     */
/*                                                                           */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
lblt(blt, clip, mouse)
BLTSTRUCT *blt;
int 	clip,	/* 1 == clipping, 0 == not */
	mouse;	/* 1 == think about it , 0 == concern not thyself */
{
	BLTSTRUCT b;
	register int i;
	register LAYER *lp;

#ifdef DEBUG
peteprint("LBLT: entry :  blt structure : \n");
peteprint("\tsrc bitmap = 0x%lx, (%d, %d) (%d, %d), %d\n", blt->src->base,
       blt->src->rect.origin.x, blt->src->rect.origin.y,
       blt->src->rect.corner.x, blt->src->rect.corner.y,
       blt->src->width);
peteprint("\tdst bitmap = 0x%lx, (%d, %d) (%d, %d), %d\n", blt->dst->base,
       blt->dst->rect.origin.x, blt->dst->rect.origin.y,
       blt->dst->rect.corner.x, blt->dst->rect.corner.y,
       blt->dst->width);
peteprint("\tdr = (%d, %d) (%d, %d),  sp = (%d, %d)\n", 
       blt->dr.origin.x, blt->dr.origin.y,
       blt->dr.corner.x, blt->dr.corner.y,
       blt->sp.x, blt->sp.y);
#endif

	b = *blt;
	lp = (LAYER *)blt->dst;

	for ( i = 0; i < MAX_LRBUF; i++ )
		if ( lp->reg[i].flag == L_VISIBLE )
		{
			b.dst = &(lp->reg[i].bm);
			bitblt(&b, clip, mouse);
		}
		else if ( lp->reg[i].flag == L_EMPTY )
			return;
		
}


lbitblt(blt, clip, mouse)
register BLTSTRUCT *blt;
int	clip, mouse;
{
	lblt(blt, clip, mouse);
}
/*

*/

LAYER *
newlayer(r)
register RECT r;
{
	register LAYER *newlp;
	
#ifdef TRACE
peteprint("newlayer\n");
#endif

	newlp = (LAYER *)malloc(sizeof(LAYER)) ;
	if (newlp == (LAYER *)NULL )
	{
		peteprint("NEWLAYER STRUCTURE ALLOCATE FAILURE\n");
		memfail();
	}
	newlp->rect = r;

	/* link newlp into the layer list */
	
	if ( DM_frontmost != (LAYER *)NULL )
		DM_frontmost->front = newlp;

	newlp->back = DM_frontmost;
	newlp->front = (LAYER *)NULL;

	DM_frontmost = newlp;

	if ( DM_rearmost == (LAYER *)NULL )
		DM_rearmost = newlp;

	make_vis_list();

	rectf(r, FILL_CLR);
	return newlp;
}


/*

*/


int dellayer(lp)
register LAYER *lp;
{

	register int i;
	int 	j,k;
	register LAYER *rlp;
	SM_REGION reg;
#ifdef TRACE
peteprint("dellayer 0x%lx\n", lp);
#endif

	/* make background those parts of the layer visible */
	for ( i = 0; i < MAX_LRBUF; i++ )
		if ( lp->reg[i].flag == L_VISIBLE )
			background(lp->reg[i].bm.rect);
	
	/* unhook lp from the list of layers */
	if ( lp == DM_frontmost ) 
		DM_frontmost = lp->back;
	else
		lp->front->back = lp->back;


	if ( lp == DM_rearmost )
		DM_rearmost = lp->front;
	else
		lp->back->front = lp->front;

	/* make the update list */
	for ( i = 0; i < MAX_UPBUF; i ++ )
	{
		update[i].wmgr = -1;
		update[i].wid = -1;
	}

	j = 0;
	for ( rlp = DM_rearmost; rlp; rlp = rlp->front )
		for ( i = 0; i < MAX_LRBUF; i++ )
	 	{
			reg = rlp->reg[i];
			if ( (reg.cov_by == lp) && (reg.flag == L_OBSCURED) )
			{
				update[j].r = reg.bm.rect;
				k = update[j].wid = lp2id(rlp);
				update[j].wmgr = wtbl[k]->wn_Wmgr;
				j++;
			}
		}


	make_vis_list();
	/* the actual perform_update in back in Delete..so the old layer */
	/* does not get outlined                                         */

#ifdef DEBUG
peteprint("dellayer: upfront and background complete\n");
peteprint("\tfr = 0x%lx, re = 0x%lx, lp = 0x%lx\n",DM_frontmost,DM_rearmost,lp);
#endif

}
/*

*/

upfront(lp)
register LAYER *lp;
{
	register int i,j;

#ifdef TRACE
peteprint("upfront 0x%lx, fr = 0x%lx, re = 0x%lx\n", lp,DM_frontmost,DM_rearmost);
#endif

	if ( lp == DM_frontmost )
		return;

	lp->front->back = lp->back;

	if ( lp->back  != (LAYER *)NULL )
		lp->back->front = lp->front;
	else
		DM_rearmost = DM_rearmost->front;

	lp->front = NULL;

	if ( DM_frontmost )
		DM_frontmost->front = lp;
	lp->back = DM_frontmost;
	DM_frontmost = lp;

	for ( i = 0; i < MAX_UPBUF; i++ )
	{
		update[i].wmgr = -1;
		update[i].wid = -1;
	}

	j = 0;
	for ( i = 0 ; i < MAX_LRBUF; i++ )
		if ( lp->reg[i].flag == L_OBSCURED )
		{
			update[j].wmgr = wtbl[lp2id(lp)]->wn_Wmgr;
			update[j].r = lp->reg[i].bm.rect;
			update[j].wid = lp2id(lp);
			j++;
		}
	make_vis_list();
	perform_update();
#ifdef DEBUG
peteprint("upfront return: fr = 0x%lx, rear = 0x%lx\n", DM_frontmost, DM_rearmost);
#endif
}

pushback(lp)
register LAYER *lp;
{
	register int i,j;
	register LAYER *tlp;
#ifdef TRACE
peteprint("pushback : entry\n");
#endif
	

	if ( DM_rearmost == lp )
		return;
	if ( DM_frontmost == lp )
	{
		DM_frontmost = lp->back;
		DM_frontmost->front = (LAYER *)NULL;
	}
	else
		lp->front->back = lp->back;

	lp->back->front = lp->front;

	for ( i = 0; i < MAX_UPBUF; i++ )
		update[i].wmgr = update[i].wid = -1;
	
	j = 0;
	for ( tlp = DM_rearmost; tlp ; tlp = tlp->front )
		for ( i = 0; i < MAX_LRBUF; i++ )
			if ((tlp->reg[i].cov_by == lp)&&(tlp->reg[i].flag == L_OBSCURED) )
			{
				update[j].wmgr = wtbl[lp2id(tlp)]->wn_Wmgr;
				update[j].r = tlp->reg[i].bm.rect;
				update[j].wid = lp2id(tlp);
				j++;
			}

	DM_rearmost->back = lp;
	lp->front = DM_rearmost;
	lp->back = (LAYER *)NULL;
	DM_rearmost = lp;
	make_vis_list();
	perform_update();

#ifdef DEBUG
peteprint("\tpushback returns\n");
#endif
	}

/*

*/
static
rectf(r, op)
register RECT r;
register int op;
{
	BITMAP s;
	register BLTSTRUCT blt;

#ifdef TRACE
peteprint("Rectf: entry\n");
#endif

	if ( op == FILL_CLR )
		blt.pat = texture[0];
	else if ( op == FILL_BCK )
		blt.pat = texture[10];
	else if ( op  == FILL_HLF )
		blt.pat = texture[3];

	s.rect = r;
	s.width = 16 * words_between(r.origin.x, r.corner.x);
	s.base = screen_addr(r.origin.x, r.origin.y);

	blt.src =  &s;
	blt.sp = s.rect.origin;
	blt.dst = &display;
	blt.dr = s.rect;
	blt.op = L_TRUE;
	bitblt(&blt);

}

background(r)
register RECT r;
{
	rectf(r, FILL_BCK);
}


int *screen_addr(x,y)
register int x,y;
	{
	register long addr;

	addr = ((long)y) << 7;	/* (long)y*128 */

	addr += ( x >> 4 ) << 1; /* addr += (x / 16) * 2 */
	
	if (addr & 0xffff0000L )
	{
		addr &= 0x0000ffffL;
		addr |= (long)SEG1;
	}
	else
		addr |= (long)SEG0;

	return ((int *)addr);
}



memfail()
{
	for(;;);
}


words_between(left,right)
register int	left,right;
{
	if (right <= left)
		return 0;
	left &= 0xfff0;
	if ((right & 0x000f) != 0)
		right = (right & 0xfff0) + 0x0010;
	return ((right - left) >> 4);
}




int *bmialloc(r)
register RECT	r;
{
	return((int*)malloc(words_between(r.origin.x,r.corner.x)
			    *(r.corner.y-r.origin.y) << 1));
}





perform_update()
	{
	register int i,j, k;

	/* now for every window to be updated, if the cursor is */
	/* on, turn the cursor off and set the flag WT_CURSOR_WON */
	/* in the flag field                                      */
	
	/* save off current window id so we can restore it to gk */

	j = gkWid;
	for ( i = 0; i < MAX_UPBUF && update[i].wmgr != -1 ; i++ )
		if ( wtbl[update[i].wid]->wn_Flags & WT_CURSOR_ON )
		{
			gk = *wtbl[update[i].wid];
			msgData1 = 0;
			SM_DrawCurs();
			gkFlags |= WT_CURSOR_WON;
			*wtbl[update[i].wid] = gk;
		}
	
	if ( legalwindow(j) && wtbl[j] != (WSTRUCT *)NULL)
		gk = *wtbl[j];

	/* The engine used to clear every update region here (rectf FILL_CLR) before
	 * firing WM_UPDATE, back when the server drew window contents.  Under Model A
	 * (GUI.md 2.9) every window is a direct-render client that owns its content
	 * pixels and repaints them on the E_EXPOSE this WM_UPDATE turns into, so that
	 * clear only blanked the region for the client to immediately clear+repaint --
	 * the visible "paints twice" on every uncover.  Dropped: the client's own
	 * repaint is the single clear, and desktop background where a window was
	 * removed/moved is filled separately (dellayer / new_dimensions call
	 * background()), so no region is left unpainted.  outline() below still redraws
	 * the server-owned frame/title bar into the region. */

	/* and then generate the update events to the managers */
	for ( i = 0; i < MAX_WINDOWS; i++ )
	  for ( j = 0; j < MAX_UPBUF; j++ )
	    if ( update[j].wid == i )
	    {
	      msgSender = wtbl[i]->wn_Wmgr;
	      msgWid = i;
	      if (not ( wtbl[i]->wn_Type & WT_SMARTUPDATE ))
	      {
		msgCmd = WM_UPDATE;
	        /*
	         * eliminate all other update events
	         * for this window -- but UNION their rects into this one first.
	         * One window can be damaged in several disjoint regions (a raise
	         * uncovers every region it was L_OBSCURED in); the receiver gets a
	         * single message, so the rect it carries must cover all of them or
	         * the parts we drop are never repainted.  A bounding union costs a
	         * little slack and keeps the common case (one strip) exact.
	         */
	        for ( k = j+1; k < MAX_UPBUF; k++ )
	          if ( update[k].wid == i )
	          {
	            if ( update[k].r.origin.x < update[j].r.origin.x )
	              update[j].r.origin.x = update[k].r.origin.x;
	            if ( update[k].r.origin.y < update[j].r.origin.y )
	              update[j].r.origin.y = update[k].r.origin.y;
	            if ( update[k].r.corner.x > update[j].r.corner.x )
	              update[j].r.corner.x = update[k].r.corner.x;
	            if ( update[k].r.corner.y > update[j].r.corner.y )
	              update[j].r.corner.y = update[k].r.corner.y;
	            update[k].wid = -1;
	          }

		/* hand the damage to the reply hook (gfxhooks.c): the MESSAGE has no
		 * room for it, and without it a client can only repaint everything. */
		gfx_uprect = update[j].r;
		outline(i);
		if ( wtbl[i]->wn_Wmgr == 3 )	/* text window */
			redraw_map(i);
		else
			sendmsg(&msg);
	      }
	      else
	      {
	        msgCmd = WM_RGNUPDATE;
	        gfx_uprect = update[i].r;	/* absolute, before the local remap */
	        update[i].r=R_subp(update[i].r,wtbl[i]->wn_Layer->rect.origin);
	        update[i].r=R_addp(update[i].r,wtbl[i]->wn_Lorigin);
	        msgPtr = &update[i].r;
	        sendmsg( &msg );
	      }
	}

	j = gkWid;
	for ( i = 0; i < MAX_WINDOWS; i++ )
		if ( wtbl[i] && wtbl[i]->wn_Flags & WT_CURSOR_WON )
			{
				gk = *wtbl[i];
				msgData1 = 1;
				SM_DrawCurs();
				gkFlags  &=  ~WT_CURSOR_WON;
				*wtbl[i] = gk;
			}
	
	if ( legalwindow(j) && wtbl[j] != ( WSTRUCT *)NULL )
		gk = *wtbl[j];

			
	}


make_vis_list()
	{
	register LAYER *back, *front;
	register int i;
	GETPTR f;

#ifdef TRACE
peteprint("Make_Vis_List\n");
#endif
	
	/* mark all buffers in all layers as not used */
	for ( back = DM_rearmost; back; back = back->front )
		for ( i = 0; i < MAX_LRBUF; i++ )
			back->reg[i].flag = L_EMPTY;

	
	for ( back = DM_rearmost; back; back = back->front )
	{
	  back->reg[0].bm.rect = back->rect;
	  back->reg[0].bm.width = 1024;
	  back->reg[0].bm.base = screen_addr(back->rect.origin.x, back->rect.origin.y);
	  back->reg[0].flag = L_VISIBLE;
	  for ( front = back->front; front; front = front->front )
	    if ( R_intersect(front->rect, back->rect) )
	      for ( i = 0; i < MAX_LRBUF; i++ )
		if ( back->reg[i].flag )
		  while ( R_intersect(front->rect, back->reg[i].bm.rect))
			if ( split(back, i, front) == 0 )
				break;
	}
	
	for ( i = 0; i < MAX_WINDOWS; i++ )
	    if ( wtbl[i] != (WSTRUCT *)NULL)
	    { 
		wtbl[i]->wn_Flags &= ~WT_FULLY_VIS;
		if ( R_equal(wtbl[i]->wn_Layer->rect, 
                             wtbl[i]->wn_Layer->reg[0].bm.rect) )
			if ( wtbl[i]->wn_Layer->reg[0].flag == L_VISIBLE )
				  wtbl[i]->wn_Flags |= WT_FULLY_VIS;
		if ( i == gkWid )
			gk = *(wtbl[i]);

	    }

}

split(lp, i, front)
register LAYER 	*lp;/* the layer containing the vis region to be broken up */
int 	i;	/* the index number of the layers vis region to break  */
LAYER 	*front;	
{
	int j;	/* the available region index for this layer           */
	RECT frect;
	RECT old,
	     new;
	register LAYER *tlp;

#ifdef TRACE
peteprint("split : entry\n");
#endif
	frect = front->rect;
	old = lp->reg[i].bm.rect;

#ifdef DEBUG
peteprint("lp = %d, i=%d, old, frect  :  \n", lp2id(lp), i);
DB_prect(old);
DB_prect(frect);
#endif
	
	if ( R_subset( old, frect ) )
	{
		/* since it is a subset of some other a higher window */
		/* it must be obscured... scan up the layer list and */
		/* find out who is the topmost obscuring             */

		lp->reg[i].flag = L_OBSCURED;
		for ( tlp = lp->front; tlp; tlp = tlp->front )
			if ( R_intersect(old, tlp->rect) )
				lp->reg[i].cov_by = tlp;
	return(0);
	}

	for (j = 0; j < MAX_LRBUF - 1; j++ )
		if ( lp->reg[j].flag == L_EMPTY )
			break;
	if ( j == (MAX_LRBUF - 1) )
	{
		peteprint("EXCEEDED RBUF FOR LAYER %d\n", lp2id(lp));
		return (0);
	}
	
	new = old;

	
	if ( old.origin.x < frect.origin.x )
		new.origin.x = old.corner.x = frect.origin.x;
	else if ( old.origin.y < frect.origin.y )
		new.origin.y = old.corner.y = frect.origin.y;
	else if ( old.corner.x > frect.corner.x )
		new.corner.x = old.origin.x = frect.corner.x;
	else if ( old.corner.y > frect.corner.y )
		new.corner.y = old.origin.y = frect.corner.y;
	else
		peteprint("WHAT THE HELL ARE YOU DOING HERE????????\n");

	lp->reg[i].bm.rect = old;
	lp->reg[i].bm.base = screen_addr(old.origin.x, old.origin.y);
	lp->reg[i].bm.width = 1024;

	/* assume visibility */
	lp->reg[i].flag = L_VISIBLE;

	/* and now scan for the highest layer which obscures it */
	for ( tlp = lp->front; tlp; tlp = tlp->front )
		if ( R_intersect(old, tlp->rect) )
		{
			lp->reg[i].flag = L_OBSCURED;
			lp->reg[i].cov_by = tlp;
		}
			
	lp->reg[j].bm.rect = new;
	lp->reg[j].bm.base = screen_addr(new.origin.x, new.origin.y);
	lp->reg[j].bm.width = 1024;
	
	/* first assume visibility */
	lp->reg[j].flag = L_VISIBLE;
	
	/* now scan for the highest layer which covers it */
	for ( tlp = lp->front; tlp; tlp = tlp->front )
		if ( R_intersect(new, tlp->rect) )
		{
			lp->reg[j].flag = L_OBSCURED;
			lp->reg[j].cov_by = tlp;
		}

#ifdef DEBUG
DB_prect(lp->reg[i].bm.rect);
DB_prect(lp->reg[j].bm.rect);
#endif
	return (1);
}








int who_top_at(p)
register POINT p;
{
	register LAYER *lp;
	
	for ( lp = DM_frontmost; lp; lp = lp->back )
		if ( R_point_in(lp->rect, p))
			return(lp2id(lp));
		
	return(NO_WINDOW);
}
	
int new_dimensions(r)
RECT r;
	{
	register LAYER *lp;
	RECT clip_rect_off;
	POINT dp_off;
	register int i,j;
	int	k;
	SM_REGION reg;
	WSTRUCT *wp;
	
	if ( r.corner.x > XMAX )
		return;
	if ( r.corner.y > YMAX )
		return;

	/* would this new location obscure any non-obscurable window ? */
/* 	COMMENTED OUT BECAUSE NO WINDOW SHOULD CURRENTLY BE UN_OBSCURABLE */
/*	AND SM_SETSIZE DEPENDS ON THIS NOT FAILING                        */
/*
	for ( i = 0; i < MAX_WINDOWS; i++ )
	  if ( i != msgWid )
	    if ( wp = wtbl[i] )
	      if ( wp->wn_Type & WT_NOOBS )
	        if ( R_intersect( r, wp->wn_Layer->rect ) )
	          return;
*/

	/* make update list */
	/* clear update table */
	for ( i = 0; i < MAX_UPBUF; i++ )
	{
		update[i].wmgr = -1;
		update[i].wid = -1;
	}

	/* initialize update table index */
	j = 0;
	for ( lp = DM_rearmost; lp; lp = lp->front )
		for ( i = 0; i < MAX_LRBUF; i++ ) 
		{
			reg = lp->reg[i];
			if  ((reg.cov_by==gkLayer)&&(reg.flag==L_OBSCURED))
			{
				update[j].r = reg.bm.rect;
				k = update[j].wid = lp2id(lp);
				update[j].wmgr = wtbl[k]->wn_Wmgr;
				j++;
			}
		}
	
	/* save off the offset from the layer origin to the clipping rect */
	clip_rect_off.origin.x = gkCrect.origin.x - gkLayer->rect.origin.x;
	clip_rect_off.origin.y = gkCrect.origin.y - gkLayer->rect.origin.y;
	clip_rect_off.corner.x = gkCrect.corner.x - gkLayer->rect.corner.x;
	clip_rect_off.corner.y = gkCrect.corner.y - gkLayer->rect.corner.y;
	/* same for drawing point */
	dp_off.x = gkDp.x - gkLayer->rect.origin.x;
	dp_off.y = gkDp.y - gkLayer->rect.origin.y;

	for ( i = 0; i < MAX_LRBUF; i++ )
		if ( gkLayer->reg[i].flag == L_VISIBLE )
			background(gkLayer->reg[i].bm.rect);

	gkLayer->rect = r;
	gkLayer->base = screen_addr(r.origin.x, r.origin.y);

	
	/* restore new clipping rectangle and drawing point */
	gkCrect.origin.x = gkLayer->rect.origin.x + clip_rect_off.origin.x;
	gkCrect.origin.y = gkLayer->rect.origin.y + clip_rect_off.origin.y;
	gkCrect.corner.x = gkLayer->rect.corner.x + clip_rect_off.corner.x;
	gkCrect.corner.y = gkLayer->rect.corner.y + clip_rect_off.corner.y;
	gkDp.x = gkLayer->rect.origin.x + dp_off.x;
	gkDp.y = gkLayer->rect.origin.y + dp_off.y;
	
	gkPsize.x = gkLayer->rect.corner.x - gkLayer->rect.origin.x;
	gkPsize.y = gkLayer->rect.corner.y - gkLayer->rect.origin.y;
	/* PTB NEW */
	*wtbl[gkWid] = gk;

	make_vis_list();

	/* add to the update list all areas of the translated layer */
	/* which are now visible                                    */
	for ( i = 0; (i < MAX_LRBUF) && (gkLayer->reg[i].flag != L_EMPTY); i++ )
	{
		reg = gkLayer->reg[i];
		if ( reg.flag == L_VISIBLE )
		{
			update[j].r = reg.bm.rect;
			update[j].wid = gkWid;
			update[j].wmgr = gkWmgr;
			j++;
		}
	}

	perform_update();
#ifdef DEBUG
peteprint("new_dimension: made it back from perform_update\n");
#endif
	
		
} 		

int lp2id(lp)
register LAYER *lp;
{
	register int i;

	for (i=0; i<MAX_WINDOWS; i++)
		if ( wtbl[i] )
			if ( wtbl[i]->wn_Layer == lp )
				return(i);
	return -1;
}


/* Region-clipped fill of absolute rect g with logical op `op', going through
 * the display bitmap (base SEG0) -- the proven blit path -- but clipped to the
 * window layer's visible regions so a covering window is never overdrawn.
 * (Blitting with a destination base of a mid-VRAM screen_addr() faults in this
 * build, so outline no longer uses lblt(&layer) as the original did.) */
static
lfillr(lp, g, op)
LAYER *lp;
RECT g;
int op;
{
	BLTSTRUCT blt;
	BITMAP s;
	SM_REGION *rp;
	RECT dr;

	if ( g.corner.x <= g.origin.x || g.corner.y <= g.origin.y )
		return;
	blt.op = op;
	blt.pat = texture[0];
	blt.dst = &display;
	for ( rp = lp->reg; rp < lp->reg + MAX_LRBUF; rp++ )
	{
		if ( rp->flag == L_EMPTY )
			break;
		if ( rp->flag != L_VISIBLE )
			continue;
		dr = R_Intersection(g, rp->bm.rect);
		if ( R_null(dr) )
			continue;
		s.rect = dr;
		s.width = 16 * words_between(dr.origin.x, dr.corner.x);
		s.base = screen_addr(dr.origin.x, dr.origin.y);
		blt.src = &s;
		blt.sp = dr.origin;
		blt.dr = dr;
		bitblt(&blt, 1, 0);
	}
}

/* Draw window i's decoration (GUI.md sec 2.7): a 1px frame around the card, a
 * white title-bar strip, and a stepped drop shadow -- all WITHIN the layer rect
 * so they clip against covering windows and are cleared when the window is
 * removed.  The WD_SHADOW-px right/bottom margin holds the shadow; the card is
 * the rest.  The shadow is a 1px black hairline at the far right/bottom edges,
 * offset inward by WD_SHADOW at the near corners (the classic Coherent stepped
 * shadow) with a white gap to the card frame -- so the window looks like it
 * floats, matching the original HR console.  The title TEXT is drawn by the
 * server (it owns the font+strings). */
outline(i)
register int i;
{
	LAYER *lp;
	RECT r, card, e;
	int k;

	lp = wtbl[i]->wn_Layer;
	r = lp->rect;
	card = r;
	card.corner.x = r.corner.x - WD_SHADOW;
	card.corner.y = r.corner.y - WD_SHADOW;

	/* clear the shadow margin (right + bottom) to white so a redraw-in-place
	 * wipes any previous shadow before the new one is stepped in */
	e = r;  e.origin.x = card.corner.x;                 lfillr(lp, e, L_TRUE);
	e = r;  e.origin.y = card.corner.y;                 lfillr(lp, e, L_TRUE);

	/* Pseudo-3D stepped drop shadow (as the original Coherent outline,
	 * _graphics/hr/src/smgr/layer.c): a solid black band down the right and along
	 * the bottom whose near edges are cut on a diagonal, so the card looks lit
	 * from the upper left and casts a stepped shadow to the lower right.  Drawn
	 * as WD_SHADOW 1px lines that step down (right band) / right (bottom band). */
	for ( k = 0; k < WD_SHADOW; k++ )
	{
		e = r;					/* right band */
		e.origin.x = card.corner.x + k;   e.corner.x = e.origin.x + 1;
		e.origin.y = r.origin.y + k;
		e.corner.y = r.corner.y - (WD_SHADOW - 1) + k;
		lfillr(lp, e, L_FALSE);
		e = r;					/* bottom band */
		e.origin.y = card.corner.y + k;   e.corner.y = e.origin.y + 1;
		e.origin.x = r.origin.x + k;
		e.corner.x = r.corner.x - (WD_SHADOW - 1) + k;
		lfillr(lp, e, L_FALSE);
	}

	/* title-bar background (white) */
	e = card;
	e.origin.x += 1;  e.origin.y += 1;
	e.corner.x -= 1;  e.corner.y = card.origin.y + WD_TITLEH;
	lfillr(lp, e, L_TRUE);

	/* 1px frame around the card.  (No separate title/content divider line: the
	 * inverted dark title bar already meets the white content with a crisp
	 * edge, and a divider here would sit on the content's top row and be
	 * overpainted by the client's background -> a stray white line.) */
	e = card;  e.corner.y = card.origin.y + 1;         lfillr(lp, e, L_FALSE);
	e = card;  e.origin.y = card.corner.y - 1;         lfillr(lp, e, L_FALSE);
	e = card;  e.corner.x = card.origin.x + 1;         lfillr(lp, e, L_FALSE);
	e = card;  e.origin.x = card.corner.x - 1;         lfillr(lp, e, L_FALSE);
}
