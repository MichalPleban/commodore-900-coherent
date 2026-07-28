#include <stdio.h>
#include "smgr.h"

#define lessptr(w1,b1,w2,b2)	(w1<w2 || (w1==w2 && b1<b2))
#define BOTTOM_UP (0)
#define TOP_DOWN  (1)

/* mouse suppression divorced from the driver (GUI.md sec 6.2): the direct
 * ioctl(myfd,CIOMSE*) calls became gfx_cursor_hide()/gfx_cursor_show(). */
extern POINT SM_Mouse_Pos;
extern bool R_point_in();

int spao, ds, ss, spat[16];
long mask2;

extern int *texture[];	/* global array of pointers to stippling patterns    */
extern int *addptr(), *subptr();


extern lf0(),  lf1(),  lf2(),  lf3(),  lf4(),  lf5(),  lf6(),  lf7();
extern lf8(),  lf9(),  lf10(), lf11(), lf12(), lf13(), lf14(), lf15();
int (*logical)();
int (*logtab[16])() =  { lf0,  lf1,  lf2,  lf3,  lf4,  lf5,  lf6,  lf7,
			 lf8,  lf9,  lf10, lf11, lf12, lf13, lf14, lf15 };

unsigned sw;
unsigned *sp;
int snb;

int *BLT_sptrw;		/* src ^                                             */
int BLT_sptrb;          /* bit position within source                        */
int BLT_sinc;		/* # of bytes to increment src ^ for next raster     */
int *BLT_dptrw;		/* dst ^                                             */
int BLT_dptrb;          /* bit position within destination                   */
int BLT_dinc;		/* # of bytes to increment dst ^ for next raster     */
int BLT_lmask;		/* BLT_left mask                                     */
int BLT_rmask;		/* right mask                                        */
int BLT_left;		/* true(1) if BLT_left partial word                  */
int BLT_right;		/* true(1) if right partial word                     */
int BLT_words;		/* # of whole words in raster line                   */
int BLT_height;         /* height ( # of rasters ) to be done                */
int BLT_width;          /* # of pixels per raster                            */
int *BLT_pat_ptr;  	/* ^ to the stippling pattern                        */
int BLT_pat_index;      /* index into the stippling pattern                  */
int (*BLT_function)();	/* function to execute                               */
int BLT_op;		/* the logical operation index (0-15)                */
int BLT_direction;      /* top-down (1) or bottom-up(0)                      */

int BLTtrue[64] = { -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1
		  };

int BLTfalse[64] = { 0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0,
		     0,  0,  0,  0,  0,  0,  0,  0
		   };

int BLT_tbl1[] = {  0x0,
		    0x0001, 0x0003, 0x0007, 0x000f, 
		    0x001f, 0x003f, 0x007f, 0x00ff,
		    0x01ff, 0x03ff, 0x07ff, 0x0fff,
		    0x1fff, 0x3fff, 0x7fff, 0xffff
		 };

int BLT_tbl2[] = {  0x0,
		    0x8000, 0xc000, 0xe000, 0xf000,
		    0xf800, 0xfc00, 0xfe00, 0xff00,
		    0xff80, 0xffc0, 0xffe0, 0xfff0,
		    0xfff8, 0xfffc, 0xfffe, 0xffff
		 };

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*                                                                         */
/* bitblt -                                                                */
/*                                                                         */
/* NOTE: this version may! not handle small overlap within                 */
/*       the same raster line                                              */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
bitblt(blt, clip, mouse)
BLTSTRUCT *blt;
int	clip,	/* 1 == yes, 0 == no */
	mouse;	/* 1 = worry about, 0 == hands off */
{
	BLTSTRUCT b;
	register int i;		/* an oft used integer variable              */

	uint	 lbits,
		 rbits,
		 *dst,
		 db1,
		 db2,
	  	 w,
		 dw;
	RECT 	srect,		/* local rectangle for the src bitmap        */
		drect;		/* local rectangle for the dst bitmap        */
	RECT	mrect;
	uint 	mouse_off;

	long long_inc;

	b = *blt;

#ifdef DEBUG
	printf("\nBITBLT--------\n");
	printf("\tb.src(handle) : 0x%lx, (%d,%d)(%d,%d) w = %d\n", b.src->base,
	    b.src->rect.origin.x, b.src->rect.origin.y,
	    b.src->rect.corner.x, b.src->rect.corner.y,
	    b.src->width);
	printf("\tb.dst(handle) : 0x%lx, (%d,%d)(%d,%d) w = %d\n", b.dst->base,
	    b.dst->rect.origin.x, b.dst->rect.origin.y,
	    b.dst->rect.corner.x, b.dst->rect.corner.y,
	    b.dst->width);
	printf("\tb.dr = (%d,%d)(%d,%d),  point = (%d,%d)  ",
	    b.dr.origin.x, b.dr.origin.y,
	    b.dr.corner.x, b.dr.corner.y,
	    b.sp.x, b.sp.y);
	printf("OP = %d\n", b.op);
#endif

	srect = b.src->rect;
	drect = b.dst->rect;

	/* if illegal b.op, bail out */
	if ( b.op > L_BLTMAX )	/* replace this with L_BLTMAX */
#ifdef DEBUG
		{
		printf("bitblt returns with bad op\n");
#endif
		return;
#ifdef DEBUG
		}
#endif

	mouse_off = 0;
	
	/* clip the x coordinates ... see SIGGRAPH '84 vol 6 */
	
	if ( clip )
	{
		i = b.sp.x - srect.origin.x;
		if ( i < 0 )
		{  
			b.sp.x -= i;
			b.dr.origin.x -= i;
		}
		i = b.dr.origin.x - drect.origin.x;
		if ( i < 0 ) 
		{ 
			b.sp.x -= i;
			b.dr.origin.x -= i;
		}
		i = b.dr.corner.x - drect.corner.x;
		if ( i > 0 )
			b.dr.corner.x -= i;
		i = b.sp.x + (b.dr.corner.x - b.dr.origin.x) - srect.corner.x;
		if ( i > 0 ) 
			b.dr.corner.x -= i; 
	
		/* clip the y coordinates...see SIGGRAPH '84 vol 6 */
		i = b.sp.y - srect.origin.y;
		if ( i < 0 )
		{ 
			b.sp.y -= i; 
			b.dr.origin.y -= i;
		}
		i = b.dr.origin.y - drect.origin.y;
		if ( i < 0 )
		{
			b.sp.y -= i;
			b.dr.origin.y -= i;
		}
		i = b.dr.corner.y - drect.corner.y;
		if ( i > 0 ) 
			b.dr.corner.y -= i; 
		i = b.sp.y + (b.dr.corner.y - b.dr.origin.y) - srect.corner.y;
		if ( i > 0 ) 
			b.dr.corner.y -= i;
	}
	BLT_width = b.dr.corner.x - b.dr.origin.x;
	BLT_height = b.dr.corner.y - b.dr.origin.y;

	/* if there is nothing left after clipping, return              */
	if ( BLT_width <= 0 || BLT_height <= 0 )
		return;
#ifdef DEBUG
	printf("\tafter clip : b.dr = (%d,%d)(%d,%d),  point = (%d,%d)\n",
	    b.dr.origin.x, b.dr.origin.y,
	    b.dr.corner.x, b.dr.corner.y,
	    b.sp.x, b.sp.y);
	printf("\theight = %d, width = %d\n", BLT_height, BLT_width);
#endif

	BLT_sptrw = b.src->base;
	BLT_sptrb = srect.origin.x & 15;  /* &15 == %16 */
	BLT_dptrw = b.dst->base;
	BLT_dptrb = drect.origin.x & 15;  /* &15 == %16 */

	long_inc = b.sp.x - srect.origin.x + 
		      b.src->width*(long)(b.sp.y-srect.origin.y);
	if ( long_inc != 0L )
		incptr(&BLT_sptrw ,&BLT_sptrb, long_inc );
	long_inc = b.dr.origin.x - drect.origin.x + 
		      b.dst->width*(long)(b.dr.origin.y - drect.origin.y);
	if ( long_inc != 0L )
		incptr(&BLT_dptrw , &BLT_dptrb, long_inc );

	BLT_sinc = (int) (b.src->width >> 3 );
	BLT_dinc = (int) (b.dst->width >> 3 );

	spao = ( b.pat == texture[0]);
	BLT_op = b.op;
	BLT_direction = TOP_DOWN;
	BLT_pat_ptr = (int*)b.pat;
	BLT_pat_index = b.dr.origin.y & 15;

	if ( BLT_width <= 16  )
	{
		if ( (b.op != L_CHAR) && 
     	     	     (lessptr(BLT_sptrw, BLT_sptrb, BLT_dptrw, BLT_dptrb) ) )
		{
			BLT_direction = BOTTOM_UP;
			long_inc = (long)BLT_height;
			long_inc -= (long)1;
			long_inc *= (long)BLT_sinc;
			BLT_sptrw = addptr(BLT_sptrw, long_inc);
			long_inc = (long)BLT_height;
			long_inc -= (long)1;
			long_inc *= (long)BLT_dinc;
			BLT_dptrw = addptr(BLT_dptrw, long_inc);
			BLT_pat_index = (BLT_pat_index + BLT_height) & 15;
			if (lessptr(BLT_dptrw,BLT_dptrb,BLT_sptrw,BLT_sptrb) )
				return;
		}
		BLT_pat_index *= 2;

#ifdef DEBUG
		printf("\t\tSMALL\n");
#endif
		if ( mouse )
		  if ( b.op == L_XOR || b.op == L_NXOR || b.op == L_NDST)
			;
		  else
		  {
			mrect = b.dr;
			mrect.origin.x -= 64;
			mrect.origin.y -= 64;
			mrect.corner.x += 64;
			mrect.corner.y += 64;
			if ( R_point_in(mrect, SM_Mouse_Pos))
			{
				mouse_off = 1;
				gfx_cursor_hide();
			}
		   }
		if ( b.op == L_CHAR )
			BLT_op = L_SRC;
		BLT_small();
#ifdef DEBUG
printf("bitblt returns\n");
#endif
		if ( mouse_off )
			gfx_cursor_show();
	        return;
	}

	db1 = 15 - (BLT_dptrb & 15);
	db2 = 15 - ((BLT_width + BLT_dptrb - 1)  & 15);

	BLT_words = ((BLT_width+BLT_dptrb-1) >> 4) - (BLT_dptrb >> 4) +
		      1 - (db1!=15?1:0) - (db2?1:0);
	
	if ( BLT_words < 0 )
		BLT_words = 0;

	BLT_lmask = ( 2 << db1) - 1 - 
		    ( BLT_width < db1+1 ? (2 << (db1-BLT_width)) -1 : 0);
	BLT_left = (db1 != 15) && (db1 < BLT_width-1);

	
	if ( db2 || BLT_width < 16 - db2 )
	{
		BLT_right = 1;
		BLT_rmask = ~((1 << db2) - 1 );
		if ( BLT_width < 16 - db2 )
			BLT_rmask -= ~((2 << db1) - 1 );
	}
	else
		BLT_right = 0;

	if ( BLT_dptrb == BLT_sptrb  || b.op == 0 || b.op == 15 )
	{
		if ( b.op == 0 || b.op == 15 )
		{
			BLT_pat_index *= 2;
			if ( mouse )
			  {
			     mouse_off = 1;
			     gfx_cursor_hide();
			  }
			BLT_block();
#ifdef DEBUG
printf("bitblt returns\n");
#endif
			if ( mouse_off )
				gfx_cursor_show();
			return;
		}
		if ( lessptr(BLT_sptrw, BLT_sptrb, BLT_dptrw, BLT_dptrb) )
		{
			BLT_direction = BOTTOM_UP;
			/* 'i' represents the increment to the last word  */
			i = BLT_left + BLT_words + BLT_right - 1;
			if ( i < 0 )
				i = 0;
			else 
				i *= 2;

			long_inc = (long)BLT_height;
			long_inc -= (long)1;
			long_inc *= (long)BLT_sinc;
			long_inc += (long)i;
			BLT_sptrw=addptr(BLT_sptrw, long_inc);
			long_inc = (long)BLT_height;
			long_inc -= (long)1;
			long_inc *= (long)BLT_dinc;
			long_inc += (long)i;
			BLT_dptrw=addptr(BLT_dptrw, long_inc);
			BLT_pat_index = (BLT_pat_index + BLT_height) & 15;
			if (lessptr(BLT_dptrw,BLT_dptrb,BLT_sptrw,BLT_sptrb) )
				return;
		}
		BLT_pat_index *= 2;
#ifdef DEBUG
		printf("\t\tgoing to full block, sptrw = 0x%lx, ", BLT_sptrw);
		printf("and  dptrw = 0x%lx\n", BLT_dptrw);
		printf("\t\tBLOCK\n");
#endif
		if ( mouse )
		  if ( b.op==L_XOR||b.op==L_NXOR||b.op==L_NDST)
			;
		  else 
		  {
		     mouse_off = 1;
		     gfx_cursor_hide();
		  }
		BLT_block();
#ifdef DEBUG
printf("bitblt returns\n");
#endif
		if ( mouse_off )
			gfx_cursor_show();
		return;
	}


#ifdef DEBUG
	printf("\t\tWORST CASE\n");
#endif
	logical = logtab[b.op];
	lbits = 15 - (BLT_sptrb & 15);
	if ( BLT_right )
		if ( BLT_width < 16 - db2 )
			rbits = db1 + 1 - db2;
		else
			rbits = 16 - db2;


	BLT_direction = TOP_DOWN;
	if ( lessptr( BLT_sptrw, BLT_sptrb, BLT_dptrw, BLT_dptrb) )
	{
		BLT_direction = BOTTOM_UP;
		long_inc = (long)BLT_height;
		long_inc -= (long) 1;
		long_inc *= (long)BLT_sinc;
		BLT_sptrw = addptr(BLT_sptrw, long_inc);
		long_inc = (long)BLT_height;
		long_inc -= (long) 1;
		long_inc *= (long) BLT_dinc;
		BLT_dptrw = addptr(BLT_dptrw, long_inc);
		BLT_pat_index = (BLT_pat_index + BLT_height) & 15;
		if ( lessptr(BLT_dptrw, BLT_dptrb, BLT_sptrw, BLT_sptrb) )
			return;
	}

	if ( mouse )
	  if ( b.op==L_XOR||b.op==L_NXOR||b.op==L_NDST)
		;
	  else 
	  {
	     mouse_off = 1;
	     gfx_cursor_hide();
	  }

	while (--BLT_height >= 0 )
	{
		sp = (unsigned *)BLT_sptrw;
		snb = lbits;
		sw = (*sp) << (15 - lbits);
		dst = (unsigned *)BLT_dptrw;
		i = BLT_words;

		if ( BLT_left )
		{
			dw = (w = *dst) & ~BLT_lmask;
			*dst = dw + ( (BLT_lmask & (*logical)(db1+1, w, 0))
				    & b.pat[BLT_pat_index] & BLT_lmask);
			dst++;
		}
		while (i-- > 0 )
			{
			*dst = (*logical)(16,*dst,0) & b.pat[BLT_pat_index];
			dst++;
			}
		if (BLT_right)
		{
			dw = ( w = *dst ) & ~BLT_rmask;
			*dst = dw +  ( (BLT_rmask & (*logical)(rbits, w, db2))
				     & b.pat[BLT_pat_index] & BLT_rmask);
		}

		if ( BLT_direction == TOP_DOWN )
		{
			long_inc = (long) BLT_sinc;
			BLT_sptrw = addptr(BLT_sptrw, long_inc);
			long_inc = (long) BLT_dinc;
			BLT_dptrw = addptr(BLT_dptrw, long_inc);
			BLT_pat_index = (BLT_pat_index + 1 ) & 15;
		}
		else
		{
			long_inc = (long)BLT_sinc;
			BLT_sptrw = subptr(BLT_sptrw, long_inc);
			long_inc = (long)BLT_dinc;
			BLT_dptrw = subptr(BLT_dptrw, long_inc);
			BLT_pat_index = (BLT_pat_index - 1 ) & 15;
		}
	}
	if ( mouse_off )
		gfx_cursor_show();
#ifdef DEBUG
printf("bitblt returns\n");
#endif
}


/*

**	Logical functions for the stupidest bitblt
*/


int lf0()
{
	return 0;
}

int lf1(bits, op1, shift)
int	bits, op1, shift;
{
	return (getbits(bits) << shift) & op1;
}

int lf2(bits, op1, shift)
int	bits, op1, shift;
{
	return (getbits(bits) << shift) & (~op1);
}

int lf3(bits, op1, shift)
int	bits, op1, shift;
{
	return  getbits(bits) << shift;
}

int lf4(bits, op1, shift)
int	bits, op1, shift;
{
	return (~(getbits(bits) << shift)) & op1;
}

int lf5(bits, op1, shift)
int	bits, op1, shift;
{
	return op1;
}

int lf6(bits, op1, shift)
int	bits, op1, shift;
{
	return (getbits(bits) << shift) ^ op1;
}

int lf7(bits, op1, shift)
int	bits, op1, shift;
{
	return (getbits(bits) << shift) | op1;
}

int lf8(bits, op1, shift)
int	bits, op1, shift;
{
	return (~(getbits(bits) << shift)) & (~op1);
}

int lf9(bits, op1, shift)
int	bits, op1, shift;
{
	return (~(getbits(bits) << shift)) ^ op1;
}

int lf10(bits, op1, shift)
int	bits, op1, shift;
{
	return ~op1;
}

int lf11(bits, op1, shift)
int	bits, op1, shift;
{
	return (getbits(bits) << shift) | (~op1);
}

int lf12(bits, op1, shift)
int	bits, op1, shift;
{
	return ~(getbits(bits) << shift);
}

int lf13(bits, op1, shift)
int	bits, op1, shift;
{
	return (~(getbits(bits) << shift)) | op1;
}

int lf14(bits,op1,shift)
int	bits,op1,shift;
{
	return (~(getbits(bits) << shift)) | (~op1);
}

int lf15()
{
	return	0xffff;
}

