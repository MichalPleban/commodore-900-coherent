#define	uchar	unsigned char
#define	ushort	unsigned short
#define	uint	unsigned
#define	ulong	unsigned long

#define	bool	char
#define	TRUE	(0 == 0)
#define	FALSE	(not TRUE)
#define	OK	0
#define	ERROR	-1
#define	not	!
#define	and	&&
#define	or	||
#define	loop	for (; ; )

#ifndef nel
#define	nel( a)		(sizeof( a) / sizeof( (a)[0]))
#endif
#define	endof( a)	(&(a)[nel( a)])
#define	ctrl( c)	((c) - 0100)
#define	tab( col)	(((col)|7) + 1)
#define	streq( s0, s1)	(strcmp( s0, s1) == 0)
#define	roundup( n, q)	((n) + (q) - (n)%(q))
#define	hiword( l)	((int) ((long)(l)>>16))
#define	loword( l)	((int) ((long)(l)&0xFFFF))
#define	makelong( h, l)	((long)(h)<<16 | (l))
#define	toascii( c)	((c) & 0177)
