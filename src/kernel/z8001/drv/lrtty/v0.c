/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <coherent.h>
#include <drvcon.h>
#include <errno.h>
#include <stat.h>
#include <tty.h>
#include <uproc.h>
#include <sched.h>
#include <signal.h>

int mmstart();

/*
 * Terminal structure.
 */
TTY	kvtty = {
	{0}, {0}, 0, mmstart, NULL, 0, 0
};

/*
 * Terminal-type name reported by the TIOCGTERM ioctl.  The low-res
 * console emulates the Heath/Zenith H-19 (Z-19), so termcap's h19
 * entry is the match.
 */
static char kvterm[TERMSZ] = "h19";

v0open(dev)
dev_t dev;
{
	register int s;

	if (minor(dev) != 0) {
		u.u_error = ENXIO;
		return;
	}
	if ((kvtty.t_flags&T_EXCL)!=0 && super()==0) {
		u.u_error = ENODEV;
		return;
	}
	ttsetgrp(&kvtty, dev);

	s = sphi();
	if (kvtty.t_open++ == 0)
	{  
	   kvtty.t_flags = T_CARR;  /* indicate "carrier" */
	   ttopen(&kvtty);
	}
	spl(s);
}

v0close(dev)
{
	register int s;

	s = sphi();
	if (--kvtty.t_open == 0)
		ttclose(&kvtty);
	spl(s);
}

v0read(dev, iop)
dev_t dev;
IO *iop;
{
	ttread(&kvtty, iop, SFCW);
}

v0write(dev, iop)
dev_t dev;
IO *iop;
{
	ttwrite(&kvtty, iop, SFCW);
}

v0ioctl(dev, com, vec)
dev_t dev;
struct sgttyb *vec;
{
	register int s;

	switch (com) {
	case TIOCGTERM:
		kucopy(kvterm, vec, TERMSZ);
		return;
	case TIOVGETB:
	case TIOVPUTB:
		while (kvtty.t_oq.cq_cc != 0) {
			s = sphi();
			if (kvtty.t_oq.cq_cc != 0) {
				kvtty.t_flags |= T_DRAIN;
				sleep((char *)&kvtty.t_oq,
					CVTTOUT, IVTTOUT, SVTTOUT);
			}
			spl(s);
			if (SELF->p_ssig && nondsig()) {
			   u.u_error = EINTR;
			   return;
			}
		}
		mmioctl(com, vec);
		return;
	}
	s = sphi();
	ttioctl(&kvtty, com, vec);
	spl(s);
}

v0load()
{
	mmload();
}
v0uload()
{
}
v0in(c)
{
	ttin(&kvtty, c);
}
