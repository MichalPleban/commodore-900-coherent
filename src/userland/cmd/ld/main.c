/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
static	flag_t	memld;			/* do in-memory load */
static	flag_t	dcomm;			/* Define commons even if reloc out */
static	flag_t	nosym;			/* Don't output symbol table */
static	flag_t	reloc;			/* Output relocation info */
static	flag_t	lmodel;			/* Large model mode */
static	flag_t	segoff;			/* Segment + offset format LR_LONG */
static	uaddr_t	base;			/* Relocation base of output */
static	char	*cbase;			/* Set by -b option */
static	struct	stat	statbuf;

static	char	*ofname = "l.out";
static	char	*entrys;		/* Entry point symbol */
static	int	digit();

FILE	*setoutput();

main(argc, argv)
int	argc;
char	*argv[];
{
	seg_t	*sgp;
	sym_t	*sp;
	mod_t	*mp;
	FILE	*ofp;
	int	i, segn;
	unsigned int	symno;
	size_t	daddr;
	uaddr_t	addr;

	scanargs(argc, argv);
	if (machine == 0)
		fatal("no input found");
	/*
	 * all modules have been read
	 * resolve meanings of various flags
	 */
	if (nundef) {			/* undefined symbols */
		nosym=0;		/* requires symbol table be retained */
		oldh.l_flag &= ~LF_SEP;	/* and cannot separate */
	} else if (!reloc) {		/* no relocation */
		dcomm++;		/* must define commons */
		if (oseg[L_REL].size!=0) {	/* discarding reloc info */
			oldh.l_flag |= LF_NRB;		/* make note */
			oseg[L_REL].size = 0;
		}
	}
	if (dcomm)		/* to define common symbols... */
		oseg[L_BSSD].size += commons;	/* we need this much room */
	if (nosym)
		oseg[L_SYM].size = 0;
	if (watch)
		message("# undefined=%d", nundef);
	/*
	 * set segment bases
	 * Finding the base here might not work for symbols
	 * as the symbol table might be not quite right yet.
	 */
	if (cbase != NULL)
		base = lentry(cbase, "relocation base");
	else if (oldh.l_flag & LF_KER)
		base = drvbase[machine];
	else
		base = userbase[machine];
	baseall(oseg, &oldh);

	ofp = setoutput();
	/*
	 * set disk offsets of segments
	 * if not bigger than a breadbox, build output file in memory;
	 * otherwise, open the file for each segment,
	 * to get independent buffering
	 */
	daddr = segoffs(oseg, 0L);
	if (watch)
		message("size: %ld", daddr);
#if BREADBOX
	if (memld||daddr<BREADBOX)
		outbuf = malloc((int)daddr);
#endif
	for (i=0; i<NLSEG; i++) {
		if (i==L_BSSI || i==L_BSSD)
			continue;
		sgp = &oseg[i];
		if (sgp->size==0)
			continue;
#if BREADBOX
		if (outbuf!=NULL) {
			int abort();

			if ((outputf[i] = malloc(sizeof(FILE)))==NULL)
				fatal("out of space");
			outputf[i]->_cc = sgp->size;
			outputf[i]->_cp = &outbuf[sgp->daddr];
			outputf[i]->_pt = &abort;
		} else
#endif
		{
			if ((outputf[i] = fopen(ofname, "r+w"))==NULL)
				fatal("cannot open %s (seg %d)", ofname, i);
			fseek(outputf[i], sgp->daddr, 0);
		}
	}
	/*
	 * define internal symbols which have been referenced
	 */
	if (dcomm) {
		i = oldh.l_flag&LF_KER;
		endbind(etext_s, L_PRVI, i);
		endbind(edata_s, L_PRVD, i);
		endbind(end_s, L_BSSD, i);
	}
	/*
	 * run through symbol table fixing everything up
	 */
	symno = 0;
	for (i=0; i<NHASH; i++) {
		for (sp=symtable[i]; sp!=NULL; sp=sp->next) {
			if ((segn=sp->s.ls_type&~L_GLOBAL) < L_SYM) {
				/* make defined symbol relative to virtual 0 */
				sp->s.ls_addr += oseg[segn].vbase;
			} else if (segn==L_SYM) {
				spmsg(sp, "references symbol segment");
			} else if (sp->s.ls_type!=(L_GLOBAL|L_REF)) {
				;	/* leave absolutes alone */
			} else if (sp->s.ls_addr==0) {
				/* undefined global */
				if (!reloc)
					spmsg(sp, "undefined");
			} else if (dcomm) {
				/* define commons */
				addr = sp->s.ls_addr;
				sp->s.ls_addr = oseg[L_BSSD].vbase
						+oseg[L_BSSD].size
						-commons;
				commons -= addr;
				sp->s.ls_type = L_GLOBAL|L_BSSD;
			}
			/* assign symbol number */
			sp->symno = symno++;
			if (!nosym) {
				/* output to symbol segment */
				sp->s.ls_addr = ptov(sp->s.ls_addr);
				canshort(sp->s.ls_type);
				canlong(sp->s.ls_addr);
				putstruc(&sp->s, sizeof sp->s,
					outputf[L_SYM], &oseg[L_SYM]);
				canshort(sp->s.ls_type);
				canlong(sp->s.ls_addr);
				sp->s.ls_addr = vtop(sp->s.ls_addr);
			}
		}
	}
	/*
	 * get entry point (cannot precede symbol table fixup)
	 */
	oldh.l_entry = entrys != NULL
			? lentry(entrys, "entry point")
			: oldh.l_flag&LF_KER	? drvbase[machine]
						: userbase[machine];
	oldh.l_entry = ptov(oldh.l_entry);
	/*
	 * Pass 2
	 */
	for (mp = modhead; mp!=NULL; mp=mp->next)
		loadmod(mp);
	/*
	 * All over but the flushing
	 */
	canshort(oldh.l_magic);
	canshort(oldh.l_flag);
	canshort(oldh.l_machine);
	canshort(oldh.l_tbase);
	canlong(oldh.l_entry);
	for (i=0; i<NLSEG; i++) {
		oldh.l_ssize[i] = oseg[i].size;
		cansize(oldh.l_ssize[i]);
	}
	if (outbuf==NULL)
		fwrite(&oldh, sizeof oldh, 1, ofp);
	else {
		for (i=0; i<sizeof oldh; i++)
			outbuf[i] = *((char *)&oldh + i);
		fwrite(outbuf, 1, (int)daddr, ofp);
	}
	if (nundef==0 || dcomm) {
		chmod(ofname, statbuf.st_mode|S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6));
		return(0);
	} else if (reloc)
		return(0);
	else
		return(nundef);
}

/*
 * Process all arguments, looking for
 * options, objects, and libraries.
 */
scanargs(ac, av)
int ac;
register char *av[];
{
	register int i;
	register char *lp;

	if (ac <= 1)
		usage("no args");
	for (i=1; i<ac; i++) {
		if (*av[i]!='-') {
			/* not -option; attempt to read file */
			if (!rdfile(av[i]))
				fatal("can't open %s", av[i]);
		} else switch (av[i][1]) {
		/* decode options */
		default:
			usage("unknown option %s", av[i]);
			continue;
		case 'd':
			dcomm++;
			continue;
		case 'e':
			if (++i >= ac)
				usage("Bad -e option");
			else
				entrys = av[i];
			continue;
		case 'i':
			oldh.l_flag |= LF_SEP;
			continue;
		case 'k':
			oldh.l_flag |= LF_KER;
			if (av[i][2]!='\0')
				rdsymbol(&av[i][2]);
			else
				rdsymbol("/coherent");
			continue;
		case 'l':
			lp = malloc(strlen(av[i])+16);
			sprintf(lp, "/lib/lib%s.a", &av[i][2]);
			if (rdfile(lp))
				;
			else {
				sprintf(lp, "/usr/lib/lib%s.a", &av[i][2]);
				if (rdfile(lp))
					;
				else
					fatal("can't open lib%s.a", &av[i][2]);
			}
			continue;
		case 'L':
			lmodel++;
			continue;
		case 'm':
			memld++;
			continue;
		case 'n':
			oldh.l_flag |= LF_SHR;
			continue;
		case 'o':
			if (++i >= ac)
				usage("bad -o option");
			else
				ofname = av[i];
			continue;
		case 'r':
			reloc++;
			continue;
		case 'R':	/* Relocation base */
			if (++i >= ac)
				usage("Bad -b option");
			else
				cbase = av[i];
			continue;
		case 's':
			nosym++;
			continue;
		case 'u':
			if (++i >= ac)
				usage("bad -u option");
			else
				undef(av[i]);
			continue;
		case 'w':
			watch++;
			message("watch!");
			continue;
		case 'X':
			noilcl++;
			continue;
		case 'x':
			nolcl++;
			continue;
		}
	}
}

/*
 * Open output file (forcefully
 * if it is necessary)
 */
static FILE *
setoutput()
{
	register FILE *fp;

	if ((fp = fopen(ofname, "w"))==NULL
	 && (unlink(ofname)!=0
	 || (fp = fopen(ofname, "w"))==NULL))
		fatal("cannot create %s", ofname);
	stat(ofname, &statbuf);
	chmod(ofname, statbuf.st_mode&~(S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6)));
	setbuf(fp, NULL);
	return (fp);
}

/*
 * Set virtual bases in array of segments
 * according to flags in header.
 * Bases are here stored as physical address
 * offsets.  This is important for seg/offset machines.
 */
void
baseall(sega, ldhp)
register seg_t	sega[];
register ldh_t	*ldhp;
{
	register uaddr_t addr;

	addr = ldhp->l_flag&LF_KER ? drvbase[ldhp->l_machine] : base;
	addr = setbase(&sega[L_SHRI], ldhp, addr);
	if ((ldhp->l_flag&(LF_SHR|LF_SEP))==LF_SHR)
		addr = setbase(&sega[L_SHRD], ldhp, addr);
	if (ldhp->l_flag&LF_SHR)
		addr = newpage(addr);
	addr = setbase(&sega[L_PRVI], ldhp, addr);
	if ((ldhp->l_flag&(LF_SHR|LF_SEP))==LF_SHR)
		addr = setbase(&sega[L_PRVD], ldhp, addr);
	addr = setbase(&sega[L_BSSI], ldhp, addr);
	if (ldhp->l_flag&LF_SEP)
		addr = seppage(ldhp, addr);
	if ((ldhp->l_flag&(LF_SHR|LF_SEP))!=LF_SHR)
		addr = setbase(&sega[L_SHRD], ldhp, addr);
	if ((ldhp->l_flag&(LF_SHR|LF_SEP))==(LF_SHR|LF_SEP))
		addr = newpage(addr);
	if ((ldhp->l_flag&(LF_SHR|LF_SEP))!=LF_SHR)
		addr = setbase(&sega[L_PRVD], ldhp, addr);
	setbase(&sega[L_BSSD], ldhp, addr);
}

/*
 * Set virtual base in given segment and return next address
 * Check for wraparound and driver space overflow
 */
uaddr_t
setbase(segp, ldhp, addr)
register seg_t	*segp;
register ldh_t	*ldhp;
register uaddr_t addr;
{
	register int m;

	m = ldhp->l_machine;
	segp->vbase = addr;
	addr += segp->size;
	if (segoff) {
		if (!lmodel && (addr-segp->vbase) > segmax[m])
			fatal("address wraparound, use `-L'");
		if (ldhp!=&oldh && segp->size>segmax[m])
			fatal("segment larger than %DKb", segmax[m]/1024);
	} else if ((unsigned)addr < (unsigned)segp->vbase)
		fatal("address wraparound");
	if ((ldhp->l_flag & LF_KER)!=0 && addr>drvtop[m])
		fatal("driver larger than %DKb", (drvtop[m]-drvbase[m])/1024);
	return (addr);
}

/*
 * Returns where to start Data
 * in a separated I/D image.
 */
uaddr_t
seppage(ldhp, addr)
register ldh_t *ldhp;
register uaddr_t addr;
{
	switch (ldhp->l_machine) {
	case M_Z8001:
		return (newpage(addr));

	default:
		return (ldhp->l_flag&LF_KER ? drvbase[ldhp->l_machine] : 0);
	}
	/* NOTREACHED */
}

/*
 * Set segment disk offsets, given sizes
 * return size of module
 */
size_t
segoffs(sega, offs)
seg_t	sega[];
register size_t	offs;
{
	register seg_t	*sgp;

	offs += sizeof(ldh_t);
	for (sgp = &sega[0]; sgp<&sega[NLSEG]; sgp++) {
		if (sgp==&sega[L_BSSI] || sgp==&sega[L_BSSD])
			continue;
		else {
			sgp->daddr = offs;
			offs += sgp->size;
		}
	}
	return (offs);
}

/*
 * Bump address to next hardware segment boundary
 * [Note: assumption that segsize table contains powers of 2]
 */
uaddr_t
newpage(addr)
uaddr_t	addr;
{
	register uaddr_t inc = segsize[machine]-1;

	return ((addr+inc)&~inc);
}

/*
 * Determine entry point; either octal address or symbol name.
 * Symbol table is already fixed-up (canonicalised) so must
 * undo it here.
 */
uaddr_t
lentry(cp, m)
register char *cp;
char *m;
{
	char	id[NCPLN], *s=&id[0];
	uaddr_t	oaddr = 0;
	int notoct = 0;
	int base = 10;
	int d;
	register sym_t	*sp;

	if (*cp == '0') {
		if (*++cp == 'x') {
			cp++;
			base = 16;
		}
	}
	for (; *cp; cp++) {
		if (s < &id[NCPLN])
			*s++ = *cp;
		if ((d = digit(*cp)) < base)
			oaddr = oaddr*base + d;
		else
			++notoct;
	}
	if (!notoct)
		return (vtop(oaddr));
	while (s < &id[NCPLN])
		*s++ = 0;
	for (sp=symtable[hash(id)]; sp!=NULL; sp=sp->next) {
		if (sp->s.ls_type&L_GLOBAL
		 && sp->s.ls_type!=(L_GLOBAL|L_REF)
		 && eq(sp->s.ls_id, id)) {
			oaddr = sp->s.ls_addr;
			return (oaddr);
		}
	}
	message("%s %.*s undefined", m, NCPLN, id);
	return (0);
}

/*
 * Return the number digit for the character
 * that forms part of a number. e.g. Input
 * of 'f' or 'F' would return 15.
 */
static int
digit(d)
register int d;
{
	if (d>='0' && d<='9')
		return (d-'0');
	if (d>='a' && d<='f')
		return (d-'a');
	if (d>='A' && d<='F')
		return (d-'F');
	return (16);
}

/*
 * Define referenced internal symbol (as end of given segment)
 */
void
endbind(sp, sn, ldrv)
register sym_t	*sp;
int	sn, ldrv;
{
	if (sp==NULL) {
		return;
	} else if (sp->s.ls_type==(L_GLOBAL|L_REF) && sp->s.ls_addr==0) {
		sp->s.ls_type = L_GLOBAL|sn;
		sp->s.ls_addr = oseg[sn].size;
	} else if (ldrv) {
		return;
	} else {
		spmsg(sp, "redefines builtin");
	}
}

/*
 * Add reference to symbol table
 */
void
undef(s)
char	*s;
{
	lds_t	lds;
	int	i;

	for (i=0; i<NCPLN; i++) {
		lds.ls_id[i] = *s;
		if (*s!='\0')
			s++;
	}
	lds.ls_type = L_GLOBAL|L_REF;
	lds.ls_addr = 0;
	addsym(&lds, NULL);
}

/*
 * Routine to convert physical address offset (number of bytes relative
 * to base) into a virtual address. 
 */
uaddr_t
ptov(a)
uaddr_t a;
{
	register short unsigned t1, t2;

	switch (machine) {
	case 0:
		fatal("Machine not set in call to ptov()");

	case M_Z8001:
		t1 = (a>>8) & 0xFF00;
		t2 = a;
		return (((long)t1<<16) + (unsigned)t2);

	default:
		return (a);
	}
	/* NOTREACHED */
}

/*
 * Routine to convert virtual address to a byte-offset
 * physical address form.
 */
uaddr_t
vtop(a)
uaddr_t a;
{
	register short unsigned t1, t2;

	switch (machine) {
	case 0:
		fatal("Machine not set in vtop()");

	case M_Z8001:
		t1 = a>>24;
		t2 = a;
		return (((long)t1<<16) + t2);

	default:
		return (a);
	}
	/* NOTREACHED */
}
