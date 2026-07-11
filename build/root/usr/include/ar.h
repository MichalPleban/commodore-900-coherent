/*
 * This is the format of the
 * header at the start of every
 * archive member. This is not
 * the same as V7. To prevent
 * confusion the magic number is
 * different.
 */
#ifndef TYPES_H
#if RSX
typedef	long	size_t;
typedef	long	time_t;
#endif
#endif

#ifndef DIRSIZ
#if RSX
#define	DIRSIZ	14
#else
#include <dir.h>
#endif
#endif

#if RSX
#include <f11.h>
#endif

#define ARMAG	0177535			/* Magic number */

struct	ar_hdr {
	char	ar_name[DIRSIZ];	/* Member name */
	time_t	ar_date;		/* Time inserted */
	short	ar_gid;			/* Group id */
	short	ar_uid;			/* User id */
	short	ar_mode;		/* Mode */
	size_t	ar_size;		/* File size */
#if RSX
	struct	ufat ar_ufat;		/* File attributes */
#endif
};
/*
 * Name of header module for ranlib
 */
#ifndef	HDRNAME
#define	HDRNAME "__.SYMDEF"
#endif
/*
 * Header module is list of all global defined symbols
 * in all load modules
 */
#ifndef	L_OUT_H
#include <l.out.h>
#endif
typedef	struct	ar_sym {
	char	ar_id[NCPLN];		/* symbol name */
	size_t	ar_off;			/* offset of load module */
} ar_sym;				/* ...from end of header module */
