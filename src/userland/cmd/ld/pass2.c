/*
 * Pass 2
 */

/*
 * Names of segments
 */
char	*sname[] = {
	"SHRI", "PRVI", "BSSI", "SHRD", "PRVD", "BSSD", "DEBUG", "SYM", "REL"
};

/*
 * Read, relocate and output segments of module
 */
void
loadmod(mp)
mod_t	*mp;
{
	seg_t	*isgp, *irsp, *osgp, *orsp;
	sym_t	*sp;
	int	opcode, relseg;
	int	segn;
	uaddr_t	addr, bias;
	unsigned int	symno;
	FILE	*ifp, *irfp, *ofp, *orfp;
	static	FILE	*inputf[NLSEG];
	static	char	*fname = NULL;

	if (mp->fname == NULL) {
		lfixup2(mp);
		return;
	}
	if (watch)
		mpmsg(mp, "loading");
	for (segn=0; segn<NLSEG; segn++) {
		if (segn!=L_REL&&outputf[segn]==NULL)
			continue;
		if (fname!=mp->fname) {
			if (fname!=NULL)
				fclose(inputf[segn]);
			if ((inputf[segn]=fopen(mp->fname, "r"))==NULL) {
				filemsg(mp->fname, "can't open (pass 2)");
				exit(1);
			}
		}
		fseek(inputf[segn], mp->seg[segn].daddr, 0);
		if (watch && mp->seg[segn].size!=0)
			message("relocating seg#%d[%06lo]@%06lo to %06lo",
				segn, (long)mp->seg[segn].size,
				(long)mp->seg[segn].vbase,
				(long)oseg[segn].vbase);
	}
	fname = mp->fname;
	irfp = inputf[L_REL];
	irsp = &mp->seg[L_REL];
	orfp = outputf[L_REL];
	orsp = &oseg[L_REL];
	while ((opcode=getbyte(irfp, irsp))!=EOF) {
		addr = getaddr(irfp, irsp);
		/* find segment in which it address lies */
		for (segn=0, isgp=&mp->seg[0]; segn<L_SYM; segn++, isgp++) {
			if (addr>=isgp->vbase && addr<isgp->vbase+isgp->size)
				break;
		}
		if (segn==L_BSSI
		 || segn==L_BSSD
		 || segn==L_SYM) {
			mpmsg(mp, "bad relocation address %06lo", (long)addr);
			continue;
		}
		ifp = inputf[segn];
		ofp = outputf[segn];
		osgp = &oseg[segn];
		/* put unrelocated stuff */
		while (isgp->vbase < addr)
			putbyte(getbyte(ifp, isgp), ofp, osgp);
		bias = 0;
		switch (relseg = opcode&LR_SEG) {
		case L_SYM:
			symno = getsymno(irfp, irsp);
			if (symno>=mp->nsym) {
				mpmsg(mp, "bad reloc. sym. no. %d", symno);
			} else if ((sp=mp->sym[symno])==NULL) {
				mpmsg(mp, "symbol %d not kept", symno);
			} else if (sp->s.ls_type==(L_GLOBAL|L_REF)) {
				if (orfp!=NULL) {
					putbyte(opcode, orfp, orsp);
					putaddr(osgp->vbase, orfp, orsp);
					putsymno(sp->symno, orfp, orsp);
				}
			} else {
				bias = sp->s.ls_addr;
				if (orfp!=NULL) {
					putbyte(sp->s.ls_type&LR_SEG
						|opcode&~LR_SEG,
						orfp, orsp);
					putaddr(osgp->vbase, orfp, orsp);
					/* keep segment size correct */
					orsp->size -= sizeof(short);
				}
			}
			break;
		case L_SHRI:
		case L_PRVI:
		case L_BSSI:
		case L_SHRD:
		case L_PRVD:
		case L_BSSD:
			bias	= oseg[relseg].vbase
				- mp->seg[relseg].vbase;
		case L_ABS:
			if (orfp!=NULL) {
				putbyte(opcode, orfp, orsp);
				putaddr(osgp->vbase, orfp, orsp);
			}
			break;
		default:
			goto BadCode;
		}
		if (opcode&LR_PCR)
			bias += isgp->vbase - osgp->vbase;
		switch (opcode&LR_OP) {
		default:
		BadCode:
			mpmsg(mp, "bad relocation opcode %03o", opcode);
			break;
		case LR_BYTE:
			bias += getbyte(ifp, isgp);
			putbyte((int)bias, ofp, osgp);
			break;
		case LR_WORD:
			bias += getword(ifp, isgp);
			putword((short)bias, ofp, osgp);
			break;
		case LR_LONG:
			bias += vtop(getlong(ifp, isgp));
#if 0
			if (segoff && (bias&0x00FF0000L)==0x00FF0000L) {
				bias &= ~0x00FF0000L;
				bias += 0x01000000L;
			}
#endif
			putlong((long)ptov(bias), ofp, osgp);
			break;
		}
	}
	/* copy remainder of segments */
	for (segn=0; segn<L_SYM; segn++) {
		register int	b;

		if ((ofp=outputf[segn])==NULL)
			continue;
		ifp = inputf[segn];
		isgp = &mp->seg[segn];
		osgp = &oseg[segn];
		while ((b=getbyte(ifp, isgp))!=EOF)
			putbyte(b, ofp, osgp);
	}
#ifdef LADDR
#endif
	/* adjust bss bases (others done by putbyte) */
	oseg[L_BSSD].vbase += mp->seg[L_BSSD].size;
	oseg[L_BSSI].vbase += mp->seg[L_BSSI].size;
}

#ifdef LADDR
/*
 * Pad out with the fake module.
 * Deal with BSS and `real' segments as well.
 */
void
lfixup2(mp)
register mod_t *mp;
{
	extern char *sname[];
	register int i;
	register FILE *ofp;
	register seg_t *osgp;
	long p;

	for (i=0; i<NXSEG; i++)
		if ((p = mp->seg[i].size) != 0)
			break;
	if (watch)
		message("paddding %s for %D bytes", sname[i], p);
	if ((ofp = outputf[i]) == NULL)
		return;
	osgp = &oseg[i];
	while (p-- > 0)
		putbyte(0, ofp, osgp);
}
#endif

/*
 * I/O routines
 */
void
putstruc(p, s, fp, sgp)
register char	*p;
register unsigned int	s;
register FILE	*fp;
register seg_t	*sgp;
{
	while (s--)
		putbyte(*p++, fp, sgp);
}

unsigned short
getword(fp, sgp)
FILE	*fp;
seg_t	*sgp;
{
	return (worder==LOHI ? getlohi(fp, sgp) : gethilo(fp, sgp));
}

unsigned long
getlong(fp, sgp)
FILE *fp;
seg_t *sgp;
{
	register unsigned short w1, w2;
	register unsigned long l;

	w1 = getword(fp, sgp);
	w2 = getword(fp, sgp);
	if (lorder == LOHI)
		l = ((long)w2<<16) + w1; else
		l = ((long)w1<<16) + w2;
	return (l);
}

unsigned short
getlohi(fp, sgp)
FILE	*fp;
seg_t	*sgp;
{
	register unsigned char	b;

	b = getbyte(fp, sgp);
	return((getbyte(fp, sgp)<<8)|b);
}

unsigned short
gethilo(fp, sgp)
FILE	*fp;
seg_t	*sgp;
{
	register unsigned char	b;

	b = getbyte(fp, sgp);
	return((b<<8)|getbyte(fp, sgp));
}

int
getbyte(fp, sgp)
FILE	*fp;
seg_t	*sgp;
{
	if (sgp->size==0)
		return (EOF);
	else {
		sgp->size--;
		sgp->vbase++;
		return (getc(fp));
	}
}

void
putword(w, fp, sgp)
unsigned short	w;
FILE	*fp;
seg_t	*sgp;
{
	if (worder==LOHI)
		putlohi(w, fp, sgp);
	else
		puthilo(w, fp, sgp);
}

/*
 * Put out a long integer.
 */
void
putlong(l, fp, sgp)
register unsigned long l;
FILE *fp;
seg_t *sgp;
{
	register unsigned short w1, w2;

	w1 = l>>16;
	w2 = l&0xFFFF;
	if (lorder == LOHI) {
		putword(w2, fp, sgp);
		putword(w1, fp, sgp);
	} else {
		putword(w1, fp, sgp);
		putword(w2, fp, sgp);
	}
}

void
putlohi(w, fp, sgp)
unsigned short	w;
FILE	*fp;
seg_t	*sgp;
{
	putbyte(w&0377, fp, sgp);
	putbyte(w>>8, fp, sgp);
}

void
puthilo(w, fp, sgp)
unsigned short	w;
FILE	*fp;
seg_t	*sgp;
{
	putbyte(w>>8, fp, sgp);
	putbyte(w&0377, fp, sgp);
}

void
putbyte(b, fp, sgp)
unsigned char	b;
FILE	*fp;
seg_t	*sgp;
{
	sgp->vbase++;
	putc(b, fp);
}

/*
 * Get a relocation address.  There has to
 * be a more efficient thing than a shift to
 * do this!!
 */
unsigned long
getaddr(fp, sgp)
FILE *fp;
seg_t *sgp;
{
	register unsigned w1, w2;

	w1 = getlohi(fp, sgp);
	w2 = getlohi(fp, sgp);
	return (((long)w1<<16) + w2);
}
