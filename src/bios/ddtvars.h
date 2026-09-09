/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The debugger/console working-variable block: one common (defined
 * in rom.c).  The debugger addresses its interior cells directly;
 * the C objects go through the fields below.
 */
#ifndef	DDTVARS_H
#define	DDTVARS_H
struct	ddtvars	{
	char	d_conhires;	/* console select: 2 = hi-res */
	char	pad1[0x27];
	char	d_mmusave[4];	/* MMU descriptor save */
	char	pad1b[2];
	int	d_fdf;		/* floppy-boot flag */
	char	pad2[0x0c];
	int	d_nerr;		/* global error counter */
	char	pad3[0x12];
};
extern struct ddtvars ddtvars;	/* defined in rom.c */
#endif
