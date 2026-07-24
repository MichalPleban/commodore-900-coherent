/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Monitor is an interface to the profil system call and to the
 * program prof.  It starts the profil if `low' is non-NULL and
 * otherwise dumps out the sttistics information and turns off
 * profileing.  The region of memory profiled is from `low' to
 * `high', not includeing `high'.  `buff' is an array of `blen'
 * shorts into which the profileing will take place.
 */
#include "mon.h"


#define MODE	0666			/* mon.out creation mode */


/*
 * Note that the list pointed to by m_flst is never empty.  It always has
 * a dummy entry on the end which has m_link field NULL.
 */
static struct m_flst	dummy[];
struct m_flst		*_fnclst	= dummy;
struct m_hdr	_mhdr;
static short		*baddr;		/* buffer address */


monitor(low, high, buff, blen)
vaddr_t	low,
	high;
short	buff[];
int	blen;
{
	if (low == NULL) {
		endmon();
		return;
	}
	baddr = buff;
	if (blen >= (high-low)/2) {
		blen = (high-low)/2;
		_mhdr.m_scale = (unsigned)-1;
	} else
		_mhdr.m_scale = ((long)blen << 17) / (high-low);
	_mhdr.m_nbins = blen;
	_mhdr.m_lowpc = low;
	/* Without getting incredibly involved, this will do */
	_mhdr.m_lowsp = ((vaddr_t)&blen) + sizeof(blen);
	_mhdr.m_hisp = _mhdr.m_lowsp;
	profil(buff, blen * sizeof *buff, low, _mhdr.m_scale);
}


static endmon()
{
	register int	fd,
			cnt;
	register struct m_flst	*flp;

	profil(NULL, 0, 0, 0);
	for (flp = _fnclst, cnt = 0; flp != dummy; flp = flp->m_link)
		++cnt;
	_mhdr.m_nfuncs = cnt;
	if ((fd=creat("mon.out", MODE)) < 0)
		return;
	write(fd, &_mhdr, sizeof _mhdr);
	for (flp = _fnclst; flp != dummy; flp = flp->m_link)
		write(fd, &flp->m_data, sizeof flp->m_data);
	write(fd, baddr, _mhdr.m_nbins * sizeof *baddr);
	close(fd);
}
