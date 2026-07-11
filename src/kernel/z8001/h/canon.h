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
 * Coherent.
 * Canonical conversion routines for the Z8001.
 */

int	_canw();
long	_canl();

#define	canshort(i)	((i)=_canw(i))
#define	canint(i)	((i)=_canw(i))
#define	canlong(l)	((l)=_canl(l))
#define	canvaddr(v)	((v)=_canl(v))
#define	cansize(s)	((s)=_canl(s))
#define	candaddr(d)	((d)=_canl(d))
#define	cantime(t)	((t)=_canl(t))
#define	candev(d)	((d)=_canw(d))
#define	canino(i)	((i)=_canw(i))
