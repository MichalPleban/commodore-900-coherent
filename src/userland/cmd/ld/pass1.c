/*
 * Pass 1:
 * Read input file
 * If not archive, add it to module list
 * If archive, add all modules that satisfy global references
 * iterating until no more can be satisfied
 * If it is not a ranlib, this must be done the hard way
 */

char	nospace[] = "out of space";
int
rdfile(fname)
char	*fname;
{
	size_t	offs, arhend;
	arh_t	arh;
	struct	stat	arstat;
	int	magic, found;
	FILE	*ifp;

	if ((ifp=fopen(fname, "r"))==NULL)
		return (0);
	fstat(fileno(ifp), &arstat);
	if (fread(&magic, sizeof magic, 1, ifp)!=1)
		fatal("can't read %s", fname);
	canshort(magic);
	if (magic!=ARMAG) {
		rewind(ifp);
		addmod(ifp, (size_t)0, fname, "");
	} else if (fread(&arh, sizeof arh, 1, ifp)==1) {
		cantime(arh.ar_date);
		cansize(arh.ar_size);
		arhend = sizeof magic+sizeof arh+arh.ar_size;
		if (strncmp(arh.ar_name, HDRNAME, DIRSIZ)!=0) {
			arhend = sizeof magic;	/* not ranlib */
		} else if (arstat.st_mtime > arh.ar_date+SLOPTIME) {
			filemsg(fname, "outdated ranlib");
		} else {
			FILE	*xfp = fopen(fname, "r");
			ars_t	ars;

			if (watch)
				filemsg(fname, "ranlib");
			do {
				found = 0;
				fseek(xfp, offs=sizeof magic+sizeof arh, 0);
				for (; offs < arhend; offs+=sizeof ars) {
					if (fread(&ars, sizeof ars, 1, xfp)!=1)
						break;
					if (symref(&ars)==NULL)
						continue;
					cansize(ars.ar_off);
					fseek(ifp, ars.ar_off+=arhend, 0);
					if (fread(&arh, sizeof arh, 1, ifp)!=1)
						break;
					found |= addmod(ifp,
						ars.ar_off+=sizeof arh,
						fname, arh.ar_name);
				}
			} while (nundef&&found);
			fclose(xfp);
			fclose(ifp);
			return (1);
		}
		do {
			found = 0;
			fseek(ifp, offs=arhend, 0);
			while (offs < arstat.st_size) {
				if (fread(&arh, sizeof arh, 1, ifp)!=1)
					break;
				cansize(arh.ar_size);
				offs += sizeof arh;
				found |= modref(ifp, offs, fname, arh.ar_name);
				fseek(ifp, offs+=arh.ar_size, 0);
			}
		} while (nundef&&found);
	}
	fclose(ifp);
	return (1);
}

/*
 * Check whether module satisfies any global references.
 * If so, add the module to the list and return flag indicating success
 */
int
modref(fp, offs, fname, mname)
FILE	*fp;
size_t	offs;
char	*fname, mname[];
{
	ldh_t	ldh;
	lds_t	lds;
	unsigned int	nsym;

	if (fread(&ldh, sizeof ldh, 1, fp)!=1) {
		modmsg(fname, mname, "can't read");
		return (0);
	}
	canldh(&ldh);
	if (ldh.l_magic!=L_MAGIC) {
		modmsg(fname, mname, "bad header");
		return (0);
	}
	if ((ldh.l_flag & LF_32) == 0) {
		modmsg(fname, mname, "not a 32-bit l.out");
		return (0);
	}
	fseek(fp, symoff(&ldh), 1);
	for (nsym=ldh.l_ssize[L_SYM]/sizeof lds; nsym; nsym--) {
		if (fread(&lds, sizeof lds, 1, fp)!=1) {
			modmsg(fname, mname, "bad symbol segment");
			return (0);
		}
		canshort(lds.ls_type);
		if (lds.ls_type&L_GLOBAL
		 && lds.ls_type!=(L_GLOBAL|L_REF)
		 && symref(&lds)!=NULL) {
			fseek(fp, offs, 0);
			return (addmod(fp, offs, fname, mname));
		}
	}
	return (0);
}

/*
 * Allocate module descriptor and add to end of list.
 */
int
addmod(fp, offs, fname, mname)
FILE	*fp;
size_t	offs;
char	*fname, mname[];
{
	mod_t	*mp;
	ldh_t	ldh;
	lds_t	lds;
	int	i, segn;
	unsigned int	nsym;
	size_t max;

	if (watch)
		modmsg(fname, mname, "adding");
	if (fread(&ldh, sizeof ldh, 1, fp)!=1) {
		modmsg(fname, mname, "can't read");
		return (0);
	}
	canldh(&ldh);
	if (ldh.l_magic!=L_MAGIC) {
		modmsg(fname, mname, "bad header");
		return (0);
	}
	if ((ldh.l_flag & LF_32) == 0) {
		modmsg(fname, mname, "not 32-bit load module");
		return (0);
	}
	if ((ldh.l_flag & LF_SLIB) != 0) {
		oldh.l_flag |= LF_SLREF;
		if (watch)
			modmsg(fname, mname, "shared library adding");
		rdsymbol(fname);
		return (0);
	}
	if (ldh.l_flag&LF_SEP) {
		modmsg(fname, mname, "cannot load separated I/D");
		return (0);
	}
	if (oldh.l_machine==0) {
		machine = ldh.l_machine;
		switch (oldh.l_machine = machine) {
		case M_PDP11:
		case M_8086:
			worder = LOHI;
			lorder = HILO;
			break;
		case M_Z8001:
			segoff++;
		case M_68000:
		case M_Z8002:
			worder = HILO;
			lorder = HILO;
			break;
		default:
			modmsg(fname, mname, "can't load %s code (yet)",
				mchname[machine]);
			exit(1);
		}
	} else if (ldh.l_machine!=machine) {
		modmsg(fname, mname, "inconsistent machine");
		return (0);
	}
	if (lmodel)
		lfixup1(&ldh);
	/*
	 * allocate descriptor, add to list, initialize
	 */
	nsym = ldh.l_ssize[L_SYM]/sizeof(lds_t);
	if ((mp=malloc(sizeof(mod_t)+nsym*sizeof(sym_t *)))==NULL)
		fatal(nospace);
	if (modhead==NULL)
		modhead = mp;
	else
		modtail->next = mp;
	modtail = mp;
	mp->next = NULL;
	mp->fname = fname;
	for (i=0; i<DIRSIZ; i++)
		mp->mname[i] = mname[0] ? mname[i] : 0;
	for (i=0; i<NLSEG; i++)
		mp->seg[i].size = ldh.l_ssize[i];
	baseall(mp->seg, &ldh);
	segoffs(mp->seg, offs);
	mp->nsym = nsym;
	/*
	 * add symbols
	 */
	fseek(fp, symoff(&ldh), 1);
	for (i=0; i < nsym; i++) {
		if (fread(&lds, sizeof lds, 1, fp)!=1) {
			modmsg(fname, mname, "bad symbol segment");
			mp->nsym = i;
			break;
		} else {
			canshort(lds.ls_type);
			canlong(lds.ls_addr);
			segn = lds.ls_type&~L_GLOBAL;
			if (segn<L_SYM)	/* make symbols segment relative */
				lds.ls_addr += oseg[segn].size
						- mp->seg[segn].vbase;
			else if (segn==L_ABS)
				/*
				 * Treat absolute symbol values as virtual
				 * addresses: converts segmented long
				 * absolutes (e.g. the kernel's
				 * u_ = 0x3F000000) to the physical form
				 * the linker computes in.  Without this,
				 * the symbol table entry collapses to 0
				 * (ptov of a segmented value) and long
				 * relocations against the symbol lose
				 * the segment byte.  vtop is the identity
				 * below 64K, so ordinary absolutes are
				 * unaffected.  Backported from the modern
				 * port (July 2026); the ld that linked
				 * the Oct 1985 kernels had this fix.
				 */
				lds.ls_addr = vtop(lds.ls_addr);
			mp->sym[i] = addsym(&lds, mp);
		}
	}
	max = segmax[machine];
	for (i=0; i<NLSEG; i++)
		if (i!=L_SYM)
			oseg[i].size += ldh.l_ssize[i];
	if (lmodel)
		baseall(oseg, &oldh);
	return (1);
}

/*
 * Do any fixup to next segment here
 * for large model.
 */
void
lfixup1(ldhp)
register ldh_t *ldhp;
{
	char *calloc();
	register int i;
	register mod_t *mp;
	extern char *sname[];
	static uaddr_t max1, max;
	uaddr_t d;

	max = segmax[machine];
	max1 = max-1;
	for (i=0; i<NXSEG; i++) {
		d = oseg[i].size & max1;
		if (d + ldhp->l_ssize[i] <= max)
			continue;
		d = max-d;
		printf("adding module of %D bytes in %s\n", d, sname[i]);
		if ((mp = (mod_t*)calloc(sizeof(*mp))) == NULL)
			fatal(nospace);
		if (modhead == NULL)		/* Can't happen ? */
			modhead = mp; else
			modtail->next = mp;
		modtail = mp;
		mp->next = NULL;
		mp->fname = NULL;
		mp->nsym = 0;
		mp->seg[i].size = d;
		mp->seg[i].vbase = 0;
		oseg[i].size += d;
	}
}

/*
 * Add symbol to symbol table
 * If match, resolve references and definitions
 * otherwise, create new symbol entry.
 * Keep track of number of undefined symbols,
 * amount of space required by commons,
 * size of output symbol segment.
 */
sym_t *
addsym(lsp, mp)
register lds_t	*lsp;
mod_t	*mp;
{
	register sym_t	*sp;
	int	h;

	h = hash(lsp->ls_id);
	if (!(lsp->ls_type&L_GLOBAL)) {
		if (nolcl || noilcl && lsp->ls_id[0] == 'L')
			return (NULL);
	} else for (sp=symtable[h]; sp!=NULL; sp=sp->next) {
		if (sp->s.ls_type&L_GLOBAL && eq(sp->s.ls_id, lsp->ls_id)) {
			if (lsp->ls_type!=(L_GLOBAL|L_REF)) {
				if (sp->s.ls_type!=(L_GLOBAL|L_REF))
					/* new def, old def */
					symredef(sp, mp);
				else if (sp->s.ls_addr == 0
				 || commdef(sp->mod, mp, lsp)) {
					/* new def, old ref */
					/* common compatible with def */
					if (sp->s.ls_addr == 0)
						nundef--;
					sp->s.ls_type = lsp->ls_type;
					sp->s.ls_addr = lsp->ls_addr;
					sp->mod = mp;
				}
			} else if (sp->s.ls_type!=(L_GLOBAL|L_REF)) {
				/* new ref, old def */
				if (lsp->ls_addr!=0)
					commdef(mp, sp->mod, &sp->s);
			} else if (sp->s.ls_addr < lsp->ls_addr) {
				/* new ref > old ref */
				if (sp->s.ls_addr==0)	/* ref becomes comm */
					nundef--;
				commons += lsp->ls_addr - sp->s.ls_addr;
				sp->s.ls_addr = lsp->ls_addr;
				sp->mod = mp;
			}
			return (sp);
		}
	}
	/*
	 * symbol not found (or is local)
	 */
	if ((sp=malloc(sizeof(sym_t)))==NULL)
		fatal("out of space");
	oseg[L_SYM].size += sizeof(lds_t);
	sp->next = symtable[h];
	symtable[h] = sp;
	sp->s = *lsp;	/* struct assign */
	sp->mod = mp;
	/*
	 * note reference to internal symbol
	 */
	if (eq(sp->s.ls_id, etext_id))
		etext_s = sp;
	else if (eq(sp->s.ls_id, edata_id))
		edata_s = sp;
	else if (eq(sp->s.ls_id, end_id))
		end_s = sp;
	else if (sp->s.ls_type==(L_GLOBAL|L_REF))
		if (sp->s.ls_addr==0)
			nundef++;
		else
			commons += (uaddr_t)sp->s.ls_addr;
	return (sp);
}

/*
 * complain about redefined symbol
 */
void
symredef(sp, mp)
sym_t	*sp;
mod_t	*mp;
{
	if (mp->mname[0]=='\0')
		spmsg(sp, "redefined in file %s", mp->fname);
	else
		spmsg(sp, "redefined in file %s: module %.*s",
			mp->fname, DIRSIZ, mp->mname);
}

/*
 * Check if common compatible with symbol definition
 */
int
commdef(cmp, dmp, lsp)
mod_t	*cmp, *dmp;
lds_t	*lsp;
{
	int	type = lsp->ls_type&~L_GLOBAL;

	if (type!=L_PRVI && type!=L_SHRI)
		return (1);
	if (dmp->mname[0]==0)
		mpmsg(cmp, "common %.*s: conflicts with code in file %s",
			NCPLN, lsp->ls_id, dmp->fname);
	else
		mpmsg(cmp, "common %.*s: conflicts with code in file %s: module %.*s",
			NCPLN, lsp->ls_id, dmp->fname, DIRSIZ, dmp->mname);
	return (0);
}

/*
 * Add symbols from object file to
 * the general symbol table as absolutes.
 */
void
rdsymbol(modnam)
char	*modnam;
{
	FILE	*fp;
	ldh_t	ldh;
	lds_t	lds;
	sym_t	*sp;
	int	i;

	if ((fp=fopen(modnam, "r"))==NULL)
		fatal("can't open %s", modnam);
	if (fread(&ldh, sizeof ldh, 1, fp)!=1)
		return;		/* allow null input file */
	canldh(&ldh);
	if (ldh.l_magic!=L_MAGIC) {
		filemsg(modnam, "bad header");
		return;
	}
	if ((ldh.l_flag & LF_32) == 0) {
		filemsg(modnam, "not 32-bit load module");
		return;
	}
	if (ldh.l_machine != machine) {
		filemsg(modnam, "inconsistent machine");
		return;
	}
	fseek(fp, symoff(&ldh), 1);
	for (i=ldh.l_ssize[L_SYM]/sizeof lds; i; i--) {
		if (fread((char *)&lds, sizeof lds, 1, fp) != 1) {
			filemsg(modnam, "bad symbol segment");
			return;
		} else {
			canshort(lds.ls_type);
			canlong(lds.ls_addr);
			if (lds.ls_type&L_GLOBAL
			 && lds.ls_type!=(L_GLOBAL|L_REF)
			 && (sp=symref(&lds))!=NULL) {
				sp->s.ls_type = L_GLOBAL | L_ABS;
				sp->s.ls_addr = vtop(lds.ls_addr);
				nundef--;
			}
		}
	}
}

/*
 * Return reference to given symbol if any
 */
sym_t *
symref(ldsp)
union {lds_t; ars_t;} *ldsp;
{
	register sym_t	*sp;

	for (sp=symtable[hash(ldsp->ls_id)]; sp!=NULL; sp=sp->next)
		if (sp->s.ls_type==(L_GLOBAL|L_REF)
		 && sp->s.ls_addr == 0
		 && eq(sp->s.ls_id, ldsp->ls_id))
			break;
	return (sp);
}

/*
 * Compute hash index into symbol table by summing chars in name
 * (mod table length)
 */
int
hash(id)
char	id[];
{
	register char	*s = &id[0];
	register int	h, i;

	for (h=i=0; *s && i<NCPLN; i++)
		h += *s++;
	return (h%NHASH);
}

/*
 * Compare two symbol names
 */
int
eq(id, jd)
char	id[], jd[];
{
	register char	*si = &id[0], *sj = &jd[0];
	register int	i;

	for (i=0; *si && *sj && i<NCPLN; i++)
		if (*si++ != *sj++)
			return (0);
	return (*si==*sj);
}

/*
 * Calculate file offset of symbol segment relative to end of header
 */
size_t
symoff(ldhp)
register ldh_t	*ldhp;
{
	register size_t	offs;
	register int	i;

	offs = ldhp->l_tbase - sizeof(*ldhp);
	for (i=0; i<L_SYM; i++)
		if (i==L_BSSI || i==L_BSSD)
			continue;
		else
			offs += ldhp->l_ssize[i];
	return (offs);
}

void
canldh(ldhp)
register ldh_t *ldhp;
{
	register size_t *sp, *ep;

	canshort(ldhp->l_magic);
	canshort(ldhp->l_flag);
	canshort(ldhp->l_machine);
	canshort(ldhp->l_tbase);
	sp = &ldhp->l_ssize[0];
	ep = &ldhp->l_ssize[NLSEG];
	while (sp < ep) {
		canlong(*sp);
		sp++;
	}
}
