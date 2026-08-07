/*
 * Copyright (c) 2026 OpenCoherent contributors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * mem - report physical memory usage.
 *
 * Walks the kernel's in-core segment queue (segmq, seg.c) the same way ps
 * walks the process list: nlist() the addresses out of /coherent, snapshot
 * the kalloc arena (where the SEG nodes live) through /dev/kmem, then chase
 * the list inside the snapshot.  The queue is kept in memory-address order
 * (seggrow depends on that to find its neighbours), so the gaps between
 * consecutive segments are exactly the free holes.  One click is 1 Kb on
 * this machine (machine.h ctob), so click counts print as Kb directly.
 *
 * User memory on the z8001 is physically contiguous per segment, so
 * fragmentation matters as much as the total: a program can fail to load
 * with plenty of free memory in scattered holes.  Hence "largest".
 *
 *	mem		one-line totals + free/largest
 *	mem -l		also list every in-core segment
 */
#include <stdio.h>
#include <l.out.h>
#include <seg.h>
#include <romconf.h>
#include <param.h>
#include <filsys.h>		/* BSIZE */

#define range(p)	((char *)(p) >= (char *)aend && \
			 (char *)(p) < (char *)aend + casize)
#define map(p)		((SEG *)&allp[(char *)(p) - (char *)aend])

#define	asegmq		nl[0].n_value
#define	acorebot	nl[1].n_value
#define	acoretop	nl[2].n_value
#define	aasize		nl[3].n_value
#define	aend		nl[4].n_value
#define	aromconf	nl[5].n_value

struct nlist nl[] ={
	"segmq_",	0,	0,
	"corebot_",	0,	0,
	"coretop_",	0,	0,
	"asize_",	0,	0,
	"end_",		0,	0,
	"romconf_",	0,	0,
	/* The terminator must be a COMPLETE initializer group: the z8001 PCC
	 * drops a trailing partial group, so a bare "" left the array one
	 * element short and nlist() scanned (and zeroed!) past its end. */
	"",		0,	0
};

int	lflag;				/* list every segment */
int	kfd;				/* /dev/kmem */
char	*allp;				/* arena snapshot */
unsigned casize;			/* arena size */

main(argc, argv)
char *argv[];
{
	register SEG *sp;
	register int i;
	SEG head;
	struct romconf rc;
	saddr_t corebot, coretop, prev;
	unsigned total, used, sharedk, savedk, stackk, systk;
	unsigned nseg, nshared, ngap, gap, biggap;
	char *cp;

	for (i = 1; i < argc; i++)
		for (cp = argv[i]; *cp; cp++)
			switch (*cp) {
			case '-':
				continue;
			case 'l':
				lflag++;
				continue;
			default:
				fprintf(stderr, "Usage: mem [-l]\n");
				exit(1);
			}

	nlist("/coherent", nl);
	if (nl[0].n_type == 0)
		fail("bad namelist in /coherent");
	if ((kfd = open("/dev/kmem", 0)) < 0)
		fail("cannot open /dev/kmem");
	kread((long)aasize, (char *)&casize, sizeof (casize));
	if ((allp = malloc(casize)) == NULL)
		fail("out of memory");
	kread((long)aend, allp, (int)casize);
	kread((long)asegmq, (char *)&head, sizeof (head));
	kread((long)acorebot, (char *)&corebot, sizeof (corebot));
	kread((long)acoretop, (char *)&coretop, sizeof (coretop));
	kread((long)aromconf, (char *)&rc, sizeof (rc));

	/* rom_bram/rom_eram are 1 Kb clicks despite romconf.h's stale
	 * "512 byte click" comment: mcheck() assigns rom_bram straight into
	 * corebot and runs it through ctob() (<<10).  The kernel reservation
	 * between the bottom of RAM and corebot holds the kernel image, the
	 * kalloc arena, the inode table, the disk buffer cache and the
	 * clists -- all fixed at boot.  The buffer figure is NBUF*BSIZE from
	 * the kernel headers we compile against, like ps's proc layout: the
	 * kernel's blockp/clistp cannot be differenced (blockp is a physical
	 * address, clistp a pfix()ed virtual one). */
	printf("machine %5uK\n", rc.rom_eram - rc.rom_bram);
	printf("kernel  %5uK  arena %uK, disk buffers %uK\n",
		corebot - rc.rom_bram,
		(unsigned)(((long)casize + 1023) / 1024),
		(unsigned)((NBUF * (long)BSIZE) / 1024));

	total = coretop - corebot;
	used = sharedk = savedk = stackk = systk = 0;
	nseg = nshared = ngap = biggap = 0;
	prev = corebot;

	if (lflag)
		printf(" BASEK SIZEK FLAGS REF\n");
	for (sp = head.s_forw; sp != (SEG *)asegmq; sp = sp->s_forw) {
		if (range(sp) == 0)
			fail("fragmented segment list (rerun)");
		sp = map(sp);
		nseg++;
		used += sp->s_size;
		if (sp->s_flags & SFSHRX) {
			nshared++;
			sharedk += sp->s_size;
			if (sp->s_urefc > 1)
				savedk += (sp->s_urefc - 1) * sp->s_size;
		}
		if (sp->s_flags & SFDOWN)
			stackk += sp->s_size;
		if (sp->s_flags & SFSYST)
			systk += sp->s_size;
		if (sp->s_mbase > prev) {
			gap = sp->s_mbase - prev;
			ngap++;
			if (gap > biggap)
				biggap = gap;
		}
		prev = sp->s_mbase + sp->s_size;
		if (lflag) {
			printf("%6u %5u %c%c%c%c  %3d\n",
				sp->s_mbase - corebot, sp->s_size,
				sp->s_flags & SFSHRX ? 'x' : '-',
				sp->s_flags & SFTEXT ? 't' : '-',
				sp->s_flags & SFDOWN ? 's' : '-',
				sp->s_flags & SFSYST ? 'k' : '-',
				sp->s_urefc);
		}
	}
	if (coretop > prev) {
		gap = coretop - prev;
		ngap++;
		if (gap > biggap)
			biggap = gap;
	}

	printf("user    %5uK\n", total);
	printf("used    %5uK  in %u segments\n", used, nseg);
	if (nshared != 0) {
		printf("shared  %5uK  in %u segments", sharedk, nshared);
		if (savedk != 0)
			printf(", saving %uK", savedk);
		printf("\n");
	}
	if (stackk != 0)
		printf("stacks  %5uK\n", stackk);
	if (systk != 0)
		printf("system  %5uK\n", systk);
	printf("free    %5uK  in %u holes, largest %uK\n",
		total - used, ngap, biggap);
	exit(0);
}

fail(s)
char *s;
{
	fprintf(stderr, "mem: %s\n", s);
	exit(1);
}

kread(s, bp, n)
long s;
char *bp;
{
	/* kernel data addresses are 16-bit offsets into /dev/kmem */
	unsigned x;

	x = s;
	s = x;
	lseek(kfd, s, 0);
	if (read(kfd, bp, n) != n)
		fail("kernel memory read error");
}
