#include <stdio.h>
#include "smgr.h"

/* The client-safe half of the old layer.c: the layer-clipped blit entry
 * (lblt/lbitblt) plus the screen addressing/allocation helpers.  Split out so
 * a direct-render CLIENT (zterm, zclock: gtext/gtext2/gfxhooks call these)
 * links only this member, not the server-only layering machinery
 * (newlayer/dellayer/upfront/... stay in layer.c and are pulled only by the
 * server).  Keep this file free of references to the layer list or update
 * queue -- that is the property that keeps it client-safe. */

extern char *malloc();

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
