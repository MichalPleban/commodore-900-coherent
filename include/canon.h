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
