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
 * Coherent running in Segmented mode
 * on the Commodore M-series Z8001.
 * System call tables.
 */
#include <coherent.h>
#include <systab.h>

/*
 * System call functions.
 */
int	unone();
int	unull();
int	uexit();
int	ufork();
int	uread();
int	uwrite();
int	uopen();
int	uclose();
int	uwait();
int	ucreat();
int	ulink();
int	uunlink();
int	uexece();
int	uchdir();
int	umknod();
int	uchmod();
int	uchown();
char	*ubrk();
int	ustat();
long	ulseek();
int	ugetpid();
int	umount();
int	uumount();
int	usetuid();
int	ugetuid();
int	ustime();
int	uptrace();
int	ualarm();
int	ufstat();
int	upause();
int	uutime();
int	uaccess();
int	unice();
int	uftime();
int	uftime();
int	usync();
int	ukill();
int	udup();
int	upipe();
int	utimes();
int	uprofil();
long	uunique();
int	usetgid();
int	ugetgid();
int	(*usignal())();
int	usload();
int	usuload();
int	uacct();
int	ulock();
int	uioctl();
int	ugetegid();
int	ugeteuid();
int	uumask();
int	uchroot();

/*
 * Machine independent system call table.
 */
struct systab sysitab[NMICALL] ={
	0,		  INT,	unone,		/*  0 = ??? */
	I,		  INT,	uexit,		/*  1 = exit */
	0,		  INT,	ufork,		/*  2 = fork */
	I+P+I,		  INT,	uread,		/*  3 = read */
	I+P+I,		  INT,	uwrite,		/*  4 = write */
	P+I,		  INT,	uopen,		/*  5 = open */
	I,		  INT,	uclose,		/*  6 = close */
	P,		  INT,	uwait,		/*  7 = wait */
	P+I,		  INT,	ucreat,		/*  8 = creat */
	P+P,		  INT,	ulink,		/*  9 = link */
	P,		  INT,	uunlink,	/* 10 = unlink */
	P+P+P,		  INT,	uexece,		/* 11 = exec */
	P,		  INT,	uchdir,   	/* 12 = chdir */
	0,		  INT,	unone,		/* 13 = ??? */
	P+I+I,		  INT,	umknod,		/* 14 = mknod */
	P+I,		  INT,	uchmod,		/* 15 = chmod */
	P+I+I,		  INT,	uchown,		/* 16 = chown */
	P,		  PTR,	ubrk,		/* 17 = break */
	P+P,		  INT,	ustat,		/* 18 = stat */
	I+L+I,		  LONG,	ulseek,		/* 19 = lseek */
	0,		  INT,	ugetpid,	/* 20 = getpid */
	P+P+I,		  INT,	umount,		/* 21 = mount */
	P,		  INT,	uumount,	/* 22 = umount */
	I,		  INT,	usetuid,	/* 23 = setuid */
	0,		  INT,	ugetuid,	/* 24 = getuid */
	P,		  INT,	ustime,		/* 25 = stime */
	I+I+P+I,	  INT,	uptrace,	/* 26 = ptrace */
	I,		  INT,	ualarm,		/* 27 = alarm */
	I+P,		  INT,	ufstat,		/* 28 = fstat */
	0,		  INT,	upause,		/* 29 = pause */
	P+P,		  INT,	uutime,		/* 30 = utime */
	0,		  INT,	unone,		/* 31 = ??? */
	0,		  INT,	unone,		/* 32 = ??? */
	P+I,		  INT,	uaccess,	/* 33 = access */
	I,		  INT,	unice,		/* 34 = nice */
	P,		  INT,	uftime,		/* 35 = ftime */
	0,		  INT,	usync,		/* 36 = sync */
	I+I,		  INT,	ukill,		/* 37 = kill */
	0,		  INT,	unone,		/* 38 = ??? */
	0,		  INT,	unone,		/* 39 = ??? */
	0,		  INT,	unone,		/* 40 = ??? */
	I+I,		  INT,	udup,		/* 41 = dup */
	P,		  INT,	upipe,		/* 42 = pipe */
	P,		  INT,	utimes,		/* 43 = times */
	P+I+I+I,	  INT,	uprofil,	/* 44 = profil */
	0,		 LONG,	uunique,	/* 45 = unique */
	I,		  INT,	usetgid,	/* 46 = setgid */
	0,		  INT,	ugetgid,	/* 47 = getgid */
	I+P,		  PTR,	usignal,	/* 48 = signal */
	0,		  INT,	unone,		/* 49 = ??? */
	0,		  INT,	unone,		/* 50 = ??? */
	P,		  INT,	uacct,		/* 51 = acct */
	I+I+I,		  INT,	unull,		/* 52 = ??? (phys) */
	I,		  INT,	ulock,		/* 53 = lock */
	I+I+P,		  INT,	uioctl,		/* 54 = ioctl */
	I+P,		  INT,	unone,		/* 55 = ??? (mpx) */
	0,		  INT,	ugetegid,	/* 56 = getegid */
	0,		  INT,	ugeteuid,	/* 57 = geteuid */
	0,		  INT,	unone,		/* 58 = ??? */
	0,		  INT,	unone,		/* 59 = ??? */
	I,		  INT,	uumask,		/* 60 = umask */
	P,		  INT,	uchroot,	/* 61 = chroot */
	0,		  INT,	unone,		/* 62 = ??? */
	0,		  INT,	unone,		/* 63 = ??? */
	I+P+P,		  INT,	usload,		/* 64 = sload */
	I,		  INT,	usuload,	/* 65 = suload */
};
