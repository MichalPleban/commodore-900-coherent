/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Unload a driver.
 * #if I8086 perform a pseudo unload of a driver linked
 * with the system image.
 */
#include <stdio.h>
#include <canon.h>
#include <drvcon.h>
#include <l.out.h>
#include <machine.h>

main(argc, argv)
char *argv[];
{
	register int n;
	register FILE *fp;
#if I8086
	register FILE *cfp;	/* CON file pointer */
#endif
	register size_t s;
	register size_t b;
	register size_t t;
	register int conflag;
	CON con;
	struct ldsym lds;
	struct ldheader ldh;
	extern int errno;
	extern char *sys_errlist[];

	if (argc < 2)
		panic("Usage: uload <driver>");
	if ((fp=fopen(argv[1], "r")) == NULL)
		panic("Cannot open %s", argv[1]);
#if I8086
	if ((cfp=fopen("/dev/kmem", "r")) == NULL)
		panic("Cannot open /dev/kmem");
#endif
	if (fread(&ldh, sizeof(ldh), 1, fp) != 1)
		panic("Not an l.out");
	canint(ldh.l_magic);
	if (ldh.l_magic != L_MAGIC)
		panic("Not an l.out");
	for (n=0; n<NLSEG; n++)
		cansize(ldh.l_ssize[n]);
	b = sizeof(ldh);
	b += ldh.l_ssize[L_SHRI] + ldh.l_ssize[L_PRVI];
	b += ldh.l_ssize[L_SHRD] + ldh.l_ssize[L_PRVD];
	conflag = 0;
	fseek(fp, (long)b, 0);
	for (s=ldh.l_ssize[L_SYM]; s; s-=sizeof(lds)) {
		if (fread(&lds, sizeof(lds), 1, fp) != 1)
			panic("Bad l.out");
		if (strcmp(&lds.ls_id[2], "con_") != 0)
			continue;
		conflag++;
		canvaddr(lds.ls_addr);
#if I8086
		fseek(cfp, (long)lds.ls_addr, 0);
#else
		t = sizeof(ldh) + lds.ls_addr - (long)dvirt();
#if Z8001
		t = (unsigned)t;
#endif
		fseek(fp, t, 0);
#endif
#if I8086
		if (fread(&con, sizeof(con), 1, cfp) != 1)
#else
		if (fread(&con, sizeof(con), 1, fp) != 1)
#endif
			panic("Cannot read configuration");
#if ! I8086
		fseek(fp, (long)b+ldh.l_ssize[L_SYM]-s+sizeof(lds), 0);
#endif
		if (suload(con.c_mind) < 0)
			fprintf( stderr, "uload: %.2s: %s\n", lds.ls_id,
				sys_errlist[errno]);
	}
	if (conflag == 0)
		panic("Configuration table not found");
}

/*
 * Print out an error message and exit.
 */
panic(a1)
char *a1;
{
	fprintf(stderr, "%r", &a1);
	fprintf(stderr, "\n");
	exit(1);
}
