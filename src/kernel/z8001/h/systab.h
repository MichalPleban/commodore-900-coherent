/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
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
