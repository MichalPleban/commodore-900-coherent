/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#define	YSCROLL		25		/* scan line displ. when scrolling */

/* texttab flags
 */
#define	ERASED		01		/* this line is empty */
#define	SCROLLABLE	02		/* resp. scantab entries are valid */

#define	MAXLINE	(YMAX / YSCROLL)	/* max # text lines per screen */
#define	MAXCOL	(XMAX / 12)		/* max # char columns per text line */


struct scanline {
	uchar	sc_off,
		sc_nword;
};


extern char		texttab[];
extern struct scanline	scantab[];
