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
	kbopen();
}

v0close(dev)
{
	register int s;

	s = sphi();
	if (--kvtty.t_open == 0)
		ttclose(&kvtty);
	spl(s);
	kbclose();
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
