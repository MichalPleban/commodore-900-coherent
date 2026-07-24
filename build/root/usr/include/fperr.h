/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Floating Point Exception codes for SIGFPE handlers
 */
#ifndef	FPERR_H
#define	FPERR_H	FPERR_H

enum	fperr	{
	FPE_DV0,		/* divide by 0 */
	FPE_UFL,		/* underflow */
	FPE_OFL,		/* overflow */
	FPE_IOF,		/* integer overflow */
	FPE_NAN,		/* illegal number */
	FPE_UNK			/* unknown */
};
enum	fpeact	{		/* fp err actions */
	FPE_ST0,		/* set result to 0 */
	FPE_SIG			/* generate a signal */
};
char	*fperrstr[] = {
		"divide by 0",
		"underflow",
		"overflow",
		"integer overflow",
		"illegal number",
		"unknown"
	};

#endif
