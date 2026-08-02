/*
 * locktest3.c - the SAME contention test as locktest2, but exercising the
 * BLOCKING drawing lock (hr_lock/hr_unlock -> hr driver CIOMLOCK/CIOMUNLOCK).
 * Loads /drv/hr first (like zview) so the slow-path ioctls exist, and dumps
 * the driver's debug counters so we see the wake path.  Correct => viol==0.
 */
#include <stdio.h>

#define TAIL	0x3b009000L
#define LOCKW	((short *)(TAIL + 0x3800))
#define COUNTP	((long  *)(TAIL + 0x3820))
#define OWNERP	((short *)(TAIL + 0x3824))
#define VIOLP	((short *)(TAIL + 0x3826))
#define DONEP	((short *)(TAIL + 0x3828))
#define DIAG	((short *)(TAIL + 0x3840))	/* [0]nwait [1]nsleep [2]nwake [3]nbreak */

extern int	hr_lock();
extern int	hr_unlock();

#define NPROC	3
#define NITER	20
#define WORK	60000

long	sink;

static
loaddrv()
{
	int pid, st;
	pid = fork();
	if ( pid == 0 ) { execl("/etc/load", "load", "/drv/hr", (char *)0); _exit(1); }
	while ( wait(&st) != pid ) ;
}

main()
{
	int	i, isparent, me, fd, bad;
	long	w;
	char	buf[160];

	loaddrv();
	*LOCKW = 0; *COUNTP = 0; *OWNERP = 0; *VIOLP = 0; *DONEP = 0;
	DIAG[0] = 0; DIAG[1] = 0; DIAG[2] = 0; DIAG[3] = 0;

	isparent = 1;
	for ( i = 0; i < NPROC - 1; i++ )
		if ( fork() == 0 ) { isparent = 0; break; }
	me = getpid() & 0x7fff;
	if ( me == 0 ) me = 1;

	for ( i = 0; i < NITER; i++ )
	{
		hr_lock(LOCKW);
		*OWNERP = me;
		bad = 0;
		for ( w = 0; w < WORK; w++ )
			if ( *OWNERP != me )
				bad = 1;
		if ( bad )
			*VIOLP = (*VIOLP + 1);
		*COUNTP = (*COUNTP + 1);
		hr_unlock(LOCKW);
	}

	if ( !isparent )
	{
		hr_lock(LOCKW); *DONEP = (*DONEP + 1); hr_unlock(LOCKW);
		_exit(0);
	}

	while ( *DONEP < NPROC - 1 )
		sink++;
	sprintf(buf, "count=%ld expect=%d viol=%d nlock=%d nsleep=%d nunlock=%d d3=%d\n",
		*COUNTP, NPROC * NITER, *VIOLP & 0xffff,
		DIAG[0] & 0xffff, DIAG[1] & 0xffff, DIAG[2] & 0xffff, DIAG[3] & 0xffff);
	fd = creat("/lockout", 0644);
	if ( fd >= 0 ) { write(fd, buf, strlen(buf)); close(fd); }
	sync();
	exit(0);
}
