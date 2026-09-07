/*
 * Copyright (c) 1985 Rico Tudor.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * masks.c - the engine's CONSTANT tables: fill patterns, blit edge masks
 * and the logical-function dispatch tables.  Nothing here is ever written.
 *
 * They are gathered in ONE unit because of where the bytes end up: this
 * file is assembled into the SHARED half of libhrgfx.sl (the Makefile
 * rewrites the compiler's .prvd to .shrd -- see the masks.o rule), while
 * everything else the library defines sits in the private half, which the
 * kernel copies into EVERY GUI process at exec.  Constants copied per
 * process are pure waste, and ~850 bytes of these were.
 *
 * The rule for this file: pure read-only data.  No code, no commons, and
 * nothing that is ever assigned to -- the shared half is READ-ONLY at
 * run time, so a write here is a SIGSEGV, not a corruption.  The Makefile
 * rule checks the first two; the third is on you.
 */

#include "graph.h"

int HALF_TONE[] = { 0x3333, 0xcccc, 0x3333, 0xcccc,
                    0x3333, 0xcccc, 0x3333, 0xcccc,
                    0x3333, 0xcccc, 0x3333, 0xcccc,
                    0x3333, 0xcccc, 0x3333, 0xcccc
		   };

int  ALL_ON[] = { 0xffff, 0xffff, 0xffff, 0xffff,
		  0xffff, 0xffff, 0xffff, 0xffff,
		  0xffff, 0xffff, 0xffff, 0xffff,
		  0xffff, 0xffff, 0xffff, 0xffff
		 };

int  STRIPE[] = { 0xc000, 0x3000, 0x0c00, 0x0300,
		  0x00c0, 0x0030, 0x000c, 0x0003,
		  0xc000, 0x3000, 0x0c00, 0x0300,
		  0x00c0, 0x0030, 0x000c, 0x0003 
		};

int  DBL_STRIPE[] = { 0xff3f, 0xff9f, 0xffcf, 0xffe7,
		      0xfff3, 0xfff9, 0xfffc, 0x7ffe,
		      0x3fff, 0x9fff, 0xcfff, 0xe7ff, 
		      0xf3ff, 0xf9ff, 0xfcff, 0xfe7f
		    };

int  CROSS_HATCH[] = { 0xffff, 0xff3f, 0xff9f, 0xffcf,
		       0xfff3, 0xfff9, 0xfffc, 0xffff,
		       0xffff, 0xfcff, 0xf9ff, 0xf3ff,
		       0xe7ff, 0xcfff, 0x9fff, 0x3fff
		    };

int  GREY[] = { 0xc0c0, 0xc0c0, 0x0c0c, 0x0c0c,
		0xc0c0, 0xc0c0, 0x0c0c, 0x0c0c,
		0xc0c0, 0xc0c0, 0x0c0c, 0x0c0c,
		0xc0c0, 0xc0c0, 0x0c0c, 0x0c0c
               };

	

int ARROWMSK[] = { 0x0003, 0x0007, 0x000f, 0x00cf,
		   0x003f, 0x003f, 0x001f, 0x000f,
		   0x0007, 0x0003, 0x0c01, 0x1e00,
		   0x7f00, 0xff80, 0xffc1, 0xffe3
		 };

int CHICKEN[] = { 0x0f00, 0x1fc0, 0x3fc0, 0x71c0,
		  0x703e, 0xe03c, 0xe038, 0xe000,
		  0xe038, 0xe03c, 0x703e, 0x71c0,
		  0x3fc0, 0x1fc0, 0x0f00, 0x0000
		  };

int  BRICK[] = { 0xffc0, 0xffc0, 0x3f30, 0x3f30,
		 0x0c0c, 0x0c0c, 0x303f, 0x303f,
		 0xc0ff, 0xc0ff, 0x033f, 0x033f,
		 0x0c0c, 0x0c0c, 0x3f03, 0x3f03
		 };

int ALL_OFF[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


/*
int OFF_WHITE[] = { 0xFEFE, 0xEFEF, 0xFEFE, 0xEFEF,
		    0xFEFE, 0xEFEF, 0xFEFE, 0xEFEF,
		    0xFEFE, 0xEFEF, 0xFEFE, 0xEFEF,
		    0xFEFE, 0xEFEF, 0xFEFE, 0xEFEF
		  };
*/

int OFF_WHITE[] = { 0x0FFF, 0xFF0F, 0xF0FF, 0xFFF0,
		    0x0FFF, 0xFF0F, 0xF0FF, 0xFFF0,
		    0x0FFF, 0xFF0F, 0xF0FF, 0xFFF0,
		    0x0FFF, 0xFF0F, 0xF0FF, 0xFFF0
	 	  };
int *texture[] = {ALL_ON, ALL_OFF, GREY, ARROWMSK, HALF_TONE, STRIPE,
		DBL_STRIPE, CROSS_HATCH, CHICKEN, BRICK, OFF_WHITE};


/* --- from bitblt.c: the blit source-substitute planes and edge masks --- */
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

/* --- the logical-function dispatch tables (bitblt.c, gline.c) --- */
extern lf0(),  lf1(),  lf2(),  lf3(),  lf4(),  lf5(),  lf6(),  lf7();
extern lf8(),  lf9(),  lf10(), lf11(), lf12(), lf13(), lf14(), lf15();
int (*logtab[16])() =  { lf0,  lf1,  lf2,  lf3,  lf4,  lf5,  lf6,  lf7,
			 lf8,  lf9,  lf10, lf11, lf12, lf13, lf14, lf15 };

extern uint	_lfalse(), _land(),   _landn(), _lsrc(),
		_lnand(),  _ldst(),   _lxor(),  _lor(),
		_lnandn(), _lnxor(),  _lndst(), _lorn(),
		_lnsrc(),  _lnor(),   _lnorn(), _ltrue();

uint	(*lnFtnTbl[L_BLTMAX])() =
{
	_lfalse,  _land,  _landn, _lsrc,
	_lnand,	  _ldst,  _lxor,  _lor,
	_lnandn,  _lnxor, _lndst, _lorn,
	_lnsrc,	  _lnor,  _lnorn, _ltrue
};
