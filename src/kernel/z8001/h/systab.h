/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Header for system call table.
 */
#ifndef SYSTAB_H
#define SYSTAB_H

/*
 * Functions types.
 */
#define VOID	0
#define	PTR	1
#define INT	2
#define LONG	3

#define	I	sizeof(int)
#define	L	sizeof(long)
#define	P	sizeof(char *)

/*
 * System call table structure.
 */
struct systab {
	char	s_alen;			/* Size of argument list */
	char	s_type;			/* Type returned by function */
	int	(*s_func)();		/* Function */
};

/*
 * System call tables.
 */
extern	struct	systab sysitab[NMICALL];
extern	struct	systab sysdtab[NMDCALL];

#endif
