#ifdef	FAKEINPUT
#define	FAKEIN	('c'<<8|13)
#endif
/*
**	Device dependant driver routines for the hi-res tty driver
*/

#include	<coherent.h>
#include	<drvcon.h>
#include	<errno.h>
#include	<stat.h>
#include	<tty.h>
#include	<uproc.h>
#include	<signal.h>

#define NMINOR 1
#define MAJOR 8

int	htstart();

/*
**	Terminal Structure
*/
TTY	hrtty = {
	{0},{0},0,htstart,NULL,0,0
};


v0load()
{
/*
		SET KEYBOARD INT HERE!!
*/
	outb(0x414,0xff);
	portst();			/* Init MMU EELCECH !! */
	ricoload();			/* Clear Screen, etc. */
}

v0uload()
{
/*
		RESET KEYBOARD INT HERE!
*/
}

v0open(dev, mode)
dev_t dev;
{
	register	int	s;
/*
**	Make sure it's a legitimate devivce no. only 0 is supported
*/
	if(minor(dev) >= NMINOR) {
		u.u_error = ENXIO;
		return;
	}
	if ((hrtty.t_flags&T_EXCL) != 0 && super() == 0) {
		u.u_error = ENODEV;
		return;
	}
	ttsetgrp(&hrtty, dev);
	s = sphi();
	if(hrtty.t_open++ == 0)
		ttopen(&hrtty);
	spl(s);
}


v0close(dev, mode)
{
	register	int	s;
	s = sphi();
	if (--hrtty.t_open == 0)
		ttclose(&hrtty);
	spl(s);
}


v0read(dev, iop)
dev_t dev;
IO *iop;
{
	ttread(&hrtty, iop, SFCW);
}

v0write(dev, iop)
dev_t dev;
IO *iop;
{
	ttwrite(&hrtty, iop, SFCW);
}

v0ioctl(dev, com, vec)
dev_t dev;
struct	sgttyb *vec;
{
	register int	s;
#ifdef	FAKEINPUT
	struct	sgttyb	fc;

	if(com == FAKEIN) {
		s = sphi();
		ukcopy(vec, &fc, sizeof(struct sgttyb));
		ttin(&hrtty, fc.sg_ispeed);	/* convention with faker */
		spl(s);
		return;
	}
#endif
	s = sphi();
	ttioctl(&hrtty, com, vec);
	spl(s);
}
v0in(c)
char c;
{
	ttin(&hrtty, c);
}
htcopy(cp)
register char *cp;
{
	register int s;
	while(*cp) {
		s = sphi();
		ttin(&hrtty, *cp++);
		spl(s);
	}
}


/*
**	Start the Output Stream.
**	Called from `ttywrite'
**	This calls the rico vt50 subroutines
*/
htstart(tp)
register TTY *tp;
{
	register int c;
	cursor();
	while(( c=ttout(tp)) >= 0) {
		rico(c);
		if((tp->t_flags&T_STOP) != 0)
			break;
	}
	cursor();
}


