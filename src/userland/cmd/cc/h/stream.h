/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * This header file contains definitions used by the stream routines
 * for UDI, COHERENT, VAX/VMS, GEMDOS, and MSDOS versions of the compiler.
 */

#ifdef	UDI
/*
 * UDI (Series III, RMX).
 */
#define	SRMODE	"rb"
#define	SWMODE	"wb"
#define	SUMODE	"r+w+b"

#if	RUNNING_LARGE
#define	pget()	((char *)lget())
#define	pput(p)	lput((p))
#else
#define	pget()	((char *)iget())
#define	pput(p)	iput((p))
#endif

extern	ival_t	iget();
extern	lval_t	lget();
extern	void	dget();
#endif

#if	defined(COHERENT) && !defined(_WIN32)
/*
 * Coherent (many machines).
 */
#define	SRMODE	"r"
#define	SWMODE	"w"
#define	SUMODE	"r+w"

#if	RUNNING_LARGE
#define pget()	((char *)lget())
#define pput(p)	lput(p)
#else
#define	pget()	((char *)iget())
#define	pput(p)	iput((p))
#endif

extern	ival_t	iget();
extern	lval_t	lget();
extern	void	dget();
#endif

#ifdef	vax
/*
 * VAX/VMS.
 */
#define	SRMODE	"r"
#define	SWMODE	"w"
#define	SUMODE	"r+w"

#define	pget()	((char *)lget())
#define	pput(p)	lput((p))

extern	ival_t	iget();
extern	lval_t	lget();
extern	int	dget();
#endif

#ifdef	GEMDOS
/*
 * GEMDOS for the Atari ST.
 */
#define	SRMODE	"rb"
#define	SWMODE	"wb"
#define	SUMODE	"rwb"

#define pget()	((char *)lget())
#define pput(p)	lput(p)
#undef	getc
#define	getc	bingetc
#undef	putc
#define	putc	binputc


extern	ival_t	iget();
extern	lval_t	lget();
extern	void	dget();
#endif

#ifdef	_WIN32
/*
 * Windows (mingw), where the host build still defines COHERENT: the arm above
 * yields to this one.  The streams between cc0, cc1 and cc2 are BINARY, so a
 * text-mode stdio turns every 0x0A written into 0x0D 0x0A and the objects are
 * garbage -- silently, since nothing else about the compile fails.
 */
#define	SRMODE	"rb"
#define	SWMODE	"wb"
#define	SUMODE	"r+b"

#if	RUNNING_LARGE
#define	pget()	((char *)lget())
#define	pput(p)	lput((p))
#else
#define	pget()	((char *)iget())
#define	pput(p)	iput((p))
#endif

extern	ival_t	iget();
extern	lval_t	lget();
extern	void	dget();
#endif

#ifdef	MSDOS
#define SRMODE "rb"
#define SWMODE "wb"
#define SUMODE "rwb"

#if	RUNNING_LARGE
#define	pget()	((char *)lget())
#define	pput(p)	lput((p))
#else
#define	pget()	((char *)iget())
#define	pput(p)	iput((p))
#endif

extern	ival_t	iget();
extern	lval_t	lget();
extern	void	dget();
#endif

/*
 * The following definitions are required if the compiler uses
 * memory buffers instead of disk files for temporaries.
 * Currently, the VAX version uses 512K memory buffers by default;
 * other versions use 0K memory buffers (i.e. disk temp files) by default.
 */
#if	TEMPBUF
extern	unsigned char	*inbuf, *inbufp, *inbufmax;
extern	unsigned char	*outbuf, *outbufp, *outbufmax;
extern	unsigned	tempsize;
#ifdef	vax
#define	TEMPSIZE 524288		/* default memory buffer size */	
#else
#define	TEMPSIZE 0
#endif
#endif

#if	SIZEOF_LARGE	/* ie, sizeof_t != ival_t */
#define zget()	((sizeof_t)lget())
#define zput(z)	lput((lval_t)(z))
#else
#define zget()	((sizeof_t)iget())
#define zput(z)	iput((ival_t)z)
#endif
