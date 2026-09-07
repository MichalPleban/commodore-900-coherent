/*
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: MIT
 */
/*
 * Coherent l.out object writer (Z8001): a 48-byte native header, then the text (SHRI), data (PRVD),
 * symbol-table and relocation sections.  Words are big-endian (Z8000 byte order).
 *
 * The MI owns the location counter (dot/dotseg) and the symbol hash (hash2); this
 * file accumulates emitted bytes per segment and, at outdone(), lays out the object.
 * outab() advances dot, so genalign/genblock/genins/genprolog need no separate
 * bookkeeping.  Relocations are recorded as words are emitted and written in
 * ascending address order (ld streams them forward per segment).
 */
#ifdef   vax
#include "INC$LIB:cc2.h"
#else
#include "cc2.h"
#endif

/* l.out relocation encoding (as-ld).  L_SHRI/L_PRVD: see cc2mch.h. */
#define	LR_SYM	7		/* LR_SEG = symbol-based relocation	*/
#define	LR_BYTE	0		/* LR_OP = relocate a single byte	*/
#define	LR_WORD	040		/* LR_OP = relocate a 16-bit offset word	*/
#define	LR_LONG	0100		/* LR_OP = relocate a 2-word seg:offset	*/
#define	L_GLOBAL 0x10		/* symbol type: global			*/
#define	L_REF	0x0A		/* | -> undefined external (0x1A)	*/

/* A deferred fixup: the word at OB(seg, off) references symbol sp (+ addend).  At
 * outdone() -- once every symbol's address/segment is known -- the word is patched and
 * a relocation recorded: a defined function -> its text offset + L_SHRI; defined data ->
 * textSize + its data offset + addend + L_PRVD; an undefined external -> addend + a
 * symbol-based relocation.*/
struct ofix { short seg; long off; SYM *sp; long addend; };
/* held in chunks of FCHUNK, like the segment bytes: no block over a few K and no
 * ceiling short of NFCHUNK*FCHUNK relocations */
#define	FCHUNK	256
#define	NFCHUNK	64			/* 16384 fixups per object */
static struct ofix	*fixc[NFCHUNK];
#define	FIX(i)	(fixc[(i) >> 8][(i) & (FCHUNK-1)])
static int		nfix, fixcap;

/* a resolved relocation record (built at outdone from the fixups) */

/* A frame-address segment byte: the byte at text offset `off' is the segment half of a
 * stack-frame address word and takes the stack segment from the link, as a byte
 * relocation against the external symbol SS (0 in a program, 0x3F in the kernel).
 * Kept apart from the symbol fixups because it needs no symbol pointer, no addend and no patching --
 * the emitted byte is already the value SS is added to. */
#define	NSFIX	4096		/* growth ceiling: keeps the array within one 64K segment */
static long	*sfixv;
static int	nsfix, sfixcap;

/* one output symbol-table entry (built at outdone) */
struct osym { SYM *sp; short typ; unsigned long addr; };

/* An intra-function absolute-JP target: the word at text offset `off' holds the target's
 * in-text offset and must carry an L_SHRI text relocation so ld adjusts it by the final text
 * base.  Recorded by genfunc when it promotes an out-of-range JR to `JP cc,addr'. */
#define	NJFIX	4096		/* growth ceiling */
static struct jfix { long off; }	*jfixv;
static int		njfix, jfixcap;
static			putrel(), putfix(), putrels();

/* the non-code segments, in object data-image order: links, pure data, strings, general
 * data (all initialized -> PRVD, real file bytes), then reserved bss (-> BSSD, a size only,
 * no file bytes; the loader zero-fills it contiguously after PRVD).  SBSS holds uninitialized
 * globals and struct-return buffers; it MUST be last so the flat address space stays
 * text|PRVD|BSSD.  A far-pointer pool literal (SLINK), a string (SSTRN) or an SBSS reservation
 * that these omit would otherwise be silently dropped. */
static short	dataseg[] = { SLINK, SPURE, SSTRN, SDATA, SBSS };
#define	NDSEG	5
#define	NPSEG	4		/* dataseg[0..NPSEG-1] are the PRVD (file-resident) segments */

/* per-segment output byte buffers, indexed by segment number (SCODE..). */
/*
 * Segment bytes are held in 8 K chunks, not one block per segment: on the C900 a
 * pass carves its heap out of 60 K arenas, and one object's text can be 64 K.
 * OB(s, off) names the byte at `off' of segment `s'.
 */
#define	OCHUNK	8192
#define	NOCHUNK	8			/* 64 K: an object's segment cannot exceed that */
static unsigned char	*ochunk[NSEG][NOCHUNK];
#define	OB(s, off)	(ochunk[s][(int)((off) >> 13)][(int)((off) & (OCHUNK-1))])
static long		olen[NSEG], ocap[NSEG];

outinit()
{
	register int	i;

	for (i = 0; i < NSEG; ++i)
		olen[i] = 0;
	nfix = 0;
	njfix = 0;
	nsfix = 0;
}

outseg(s)
{
	/* the MI sets dotseg (and restores dot) before emitting; outab() uses dotseg. */
}

static
ogrow(s, n)
{
	while (olen[s]+n > ocap[s]) {
		register int	c;

		c = (int)(ocap[s] / OCHUNK);
		if (c >= NOCHUNK)
			cfatal("outcoh: segment larger than 64K");
		if ((ochunk[s][c] = (unsigned char *)malloc(OCHUNK)) == NULL)
			cfatal("outcoh: out of memory");
		ocap[s] += OCHUNK;
	}
}

/*
 * Emit one byte into the current segment and advance the location counter.
 */
outab(b)
{
	ogrow(dotseg, 1);
	OB(dotseg, olen[dotseg]) = b;
	++olen[dotseg];
	++dot;
}

/*
 * Emit a 16-bit word, big-endian.
 */
outw(w)
{
	outab((w >> 8) & 0xFF);
	outab(w & 0xFF);
}

/*
 * Record that the word most recently emitted into the current segment references
 * symbol `sp' (plus addend).  Resolved and turned into a relocation at outdone().
 */
outfix(sp, addend)
SYM	*sp;
long	addend;
{
	if (nfix >= fixcap) {
		if (fixcap >= NFCHUNK*FCHUNK)
			cfatal("outcoh: too many relocations");
		if ((fixc[fixcap/FCHUNK] = (struct ofix *)
		    malloc((unsigned)(FCHUNK*sizeof(struct ofix)))) == NULL)
			cfatal("outcoh: out of memory");
		fixcap += FCHUNK;
	}
	FIX(nfix).seg = dotseg;
	FIX(nfix).off = olen[dotseg] - 2;	/* the word just written */
	FIX(nfix).sp = sp;
	FIX(nfix).addend = addend;
	++nfix;
}

/*
 * Record that the word just written into the text is the first (segment-bearing) word of
 * a stack-frame address: its high byte takes the stack segment from the link.
 */
outfixss()
{
	if (nsfix >= sfixcap) {
		if (nsfix >= NSFIX)
			cfatal("outcoh: too many frame references");
		sfixcap = (sfixcap == 0) ? 256 : sfixcap*2;
		sfixv = (long *)(sfixv == NULL
			? malloc((unsigned)(sfixcap*sizeof(long)))
			: realloc((char *)sfixv, (unsigned)(sfixcap*sizeof(long))));
		if (sfixv == NULL)
			cfatal("outcoh: out of memory");
	}
	sfixv[nsfix++] = olen[SCODE] - 2;	/* the high byte of the word just written */
}

/* patch one byte already emitted into the text segment (used to fill a JR displacement
 * once the target label's address is known). */
outpatchb(off, b)
long	off;
{
	OB(SCODE, off) = b & 0xFF;
}

/* patch a big-endian word already emitted into the text segment. */
outpatchw(off, w)
long	off;
{
	OB(SCODE, off)   = (w >> 8) & 0xFF;
	OB(SCODE, off+1) = w & 0xFF;
}

/* the current fixup count / text length -- captured by genfunc so a span-promotion retry can
 * discard the emitted bytes and fixups of the aborted pass. */
int
outfixmark()
{
	return (nfix);
}

outrewind(off, fixmark)
long	off;
{
	olen[SCODE] = off;
	nfix = fixmark;
	/* the frame-segment fixups of the aborted pass: recorded in ascending text
	 * order, so everything at or past the rewind point belongs to it. */
	while (nsfix > 0 && sfixv[nsfix-1] >= off)
		--nsfix;
}

/* Record that the word at text offset `off' holds an absolute intra-function code address (a
 * promoted JP target); write the target's in-text offset and mark it for an L_SHRI text
 * relocation at outdone (so ld adjusts it by the final text base). */
outjptext(off, textoff)
long	off, textoff;
{
	if (njfix >= jfixcap) {
		if (njfix >= NJFIX)
			cfatal("outcoh: too many long jumps");
		jfixcap = (jfixcap == 0) ? 64 : jfixcap*2;
		jfixv = (struct jfix *)(jfixv == NULL
			? malloc((unsigned)(jfixcap*sizeof(struct jfix)))
			: realloc((char *)jfixv, (unsigned)(jfixcap*sizeof(struct jfix))));
		if (jfixv == NULL)
			cfatal("outcoh: out of memory");
	}
	outpatchw(off, (int)textoff);
	jfixv[njfix].off = off;
	++njfix;
}

/* the current text-segment byte offset (= the location of the next word). */
long
outhere()
{
	return (olen[SCODE]);
}

/* debug-table hooks: object mode emits no debug records. */
outdlab(refnum, class) {}
outdloc(refnum) {}

/* 4-byte PDP-canonical field: high word first, each word little-endian --
 * the native l.out/n.out byte order (ROM boot, kernel exec, shipped .o). */
static
canput(v)
register unsigned long	v;
{
	putc((int)((v>>16)&0xFF), ofp); putc((int)((v>>24)&0xFF), ofp);
	putc((int)(v&0xFF), ofp);       putc((int)((v>>8)&0xFF), ofp);
}

/* 2-byte little-endian header/symbol field (PDP-canonical short). */
static
leput(w)
{
	putc(w&0xFF, ofp);
	putc((w>>8)&0xFF, ofp);
}

static
beput(w)
{
	putc((w>>8)&0xFF, ofp);
	putc(w&0xFF, ofp);
}

/* patch a big-endian word into a segment buffer */
static
patch(seg, off, w)
long	off;
{
	OB(seg, off) = (w>>8)&0xFF;
	OB(seg, off+1) = w&0xFF;
}

/*
 * Lay a resolved value into a 2-word segmented address operand: the segment word at
 * off-2, the offset word at off.
 *
 * A NEGATIVE value cannot go in the offset word alone.  `a[i-1]' folds -1 into the
 * array's base, and ld relocates by flattening the operand (vtop: segment byte ->
 * bits 16..23), adding the symbol and converting back (ptov) -- so an offset word of
 * 0xFFFF with a segment byte that took no borrow reads as +65535, and ld's addition
 * carries UP out of the offset and lands the reference one segment too high.  Taking
 * the borrow in the segment byte makes the whole 24-bit flat value -1, and it
 * propagates correctly through ld's sum.
 *
 * A value that merely EXCEEDS 0xFFFF is left alone: the offset word takes it modulo
 * 64K and the segment word keeps the form the operand was emitted with.  That is not
 * an oversight -- ld carries the high part in its own bias, and widening this case to
 * "obviously" match the negative one double-counted it and hung ifconfig against a
 * live inetd (BRINGUP 7.47).
 *
 * The emitted segment word supplies the operand's own form (0x8000 for an
 * instruction's address operand, which is segmented-mode; 0 for a far-pointer datum),
 * so the borrow is taken against whatever is there.
 */
static
patchaddr(seg, off, val)
long	off, val;
{
	register unsigned long	flat;
	register int		sw;

	if (val >= 0) {
		patch(seg, off, (int)(val&0xFFFF));
		return;
	}
	sw = (OB(seg, off-2)&0xFF)<<8 | (OB(seg, off-1)&0xFF);
	flat = (((unsigned long)((sw>>8)&0xFF) << 16) + val) & 0xFFFFFFL;
	OB(seg, off-2) = (int)(flat>>16)&0xFF;
	OB(seg, off-1) = sw&0xFF;
	patch(seg, off, (int)(flat&0xFFFF));
}

/*
 * A common: global, never defined here, parked in SBSS by cc2.c's COMM case.
 */
static int
iscommon(sp) register SYM *sp;
{
	return (sp->s_flag&(S_GBL|S_DEF|S_LABNO)) == S_GBL && sp->s_seg == SBSS;
}

/*
 * Write the finished object.  Defined globals first (L_GLOBAL), then the undefined
 * externals (L_GLOBAL|L_REF) -- matching the Go n2's symbol order.  A text symbol's
 * address is its in-segment offset; a data symbol's is textSize + its offset.  Then the
 * fixups are resolved (patch the referencing word + emit a relocation) and the
 * relocations written in ascending address order.
 */
outdone()
{
	register SYM	*sp;
	register int	i, j;
	long		off;		/* a byte offset: an object's text can pass 32 K, so never an int */
	int		pass, nsym, nsymcap, sssym;
	long		textsize, datasize, bsssize, symsize, relsize;
	long		segbase[NSEG];
	struct osym	*syms;

	/* Data-image layout: lay the non-code segments contiguously after the text, each with
	 * a base offset within the combined image.  Their bytes were accumulated in obuf[seg];
	 * seg[seg].s_dot is the authoritative size (bss grows it via genblock without emitting
	 * bytes).  Flush the currently-open segment's location counter first. */
	if (dotseg >= 0)
		seg[dotseg].s_dot = dot;
	textsize = olen[SCODE];
	for (i = 0; i < NSEG; ++i)
		segbase[i] = 0;
	/* Lay the data segments contiguously in the flat per-object address space (PRVD then
	 * BSSD), so symbol/relocation addresses are text|PRVD|BSSD offsets.  datasize is the
	 * file-resident PRVD size (initialized segments only); bsssize is the reserved BSSD
	 * tail, which occupies NO file bytes -- ld/the loader zero-fills it. */
	datasize = 0;
	for (i = 0; i < NPSEG; ++i) {
		segbase[dataseg[i]] = datasize;
		/* Word-align each segment's size: the Z8000 masks the low address bit on
		 * word/long access, so an odd-size byte segment (e.g. an odd-length string in
		 * SSTRN) would misalign the following word/long data (SDATA) -- and an odd total
		 * PRVD shifts the NEXT linked object's data (its far pointers read one byte off). */
		datasize += (seg[dataseg[i]].s_dot + 1) & ~1;
	}
	segbase[SBSS] = datasize;		/* bss immediately follows PRVD in the image */
	bsssize = (seg[SBSS].s_dot + 1) & ~1;
						/* keep the next object's data word-aligned */

	/* Size the symbol and relocation arrays exactly: one entry per defined global,
	 * at most one undefined external per fixup, one relocation per fixup and per
	 * promoted JP target. */
	nsymcap = 1;
	for (i = 0; i < NSHASH; ++i)
		for (sp = hash2[i]; sp != NULL; sp = sp->s_fp)
			if ((sp->s_flag&(S_GBL|S_DEF|S_LABNO)) == (S_GBL|S_DEF)
			 || iscommon(sp))
				++nsymcap;
	nsymcap += nfix + 1;			/* + SS, if any frame address needs it */
	syms = (struct osym *)malloc((unsigned)(nsymcap*sizeof(struct osym)));
	if (syms == NULL)
		cfatal("outcoh: out of memory");

	/* symbol table: defined globals first (L_GLOBAL), then the undefined externals that
	 * relocations reference (L_GLOBAL|L_REF, in fixup order)*/
	nsym = 0;
	for (i = 0; i < NSHASH; ++i)
		for (sp = hash2[i]; sp != NULL; sp = sp->s_fp) {
			if ((sp->s_flag&(S_GBL|S_DEF|S_LABNO)) != (S_GBL|S_DEF))
				continue;
			if (nsym >= nsymcap)
				cfatal("outcoh: symbol table overflow");
			syms[nsym].sp = sp;
			/* encode the defining segment in the type so ld resolves a CROSS-object
			 * reference into the right output segment: code -> SHRI (bare L_GLOBAL),
			 * initialized data -> L_PRVD, bss -> L_BSSD.  (Tagging data as SHRI placed a
			 * cross-TU data symbol -- e.g. libc's _stdout -- inside the text region.) */
			syms[nsym].typ = (sp->s_seg == SCODE) ? L_GLOBAL
				: (sp->s_seg == SBSS) ? (L_GLOBAL|L_BSSD) : (L_GLOBAL|L_PRVD);
			syms[nsym].addr = (sp->s_seg == SCODE)
				? sp->s_value : textsize + segbase[sp->s_seg] + sp->s_value;
			++nsym;
		}
	/* commons: a tentative definition (`int x;', no initializer) under VCOMM,
	 * which cc1 sends as COMM and cc2.c parks in SBSS with the size as its value
	 * and no S_DEF.  On disk it is the shape as and ld agree on: an undefined
	 * external (L_GLOBAL|L_REF) whose value is its SIZE.  ld sums them into BSSD
	 * and defines each once however many objects declare it, and a real definition
	 * anywhere wins.  (They were written as plain undefined with value 0, so every
	 * uninitialized global compiled by the driver -- which sets VCOMM -- failed to
	 * link; the host wrapper never sets VCOMM, so it never saw one.) */
	for (i = 0; i < NSHASH; ++i)
		for (sp = hash2[i]; sp != NULL; sp = sp->s_fp) {
			if (!iscommon(sp))
				continue;
			if (nsym >= nsymcap)
				cfatal("outcoh: symbol table overflow");
			syms[nsym].sp = sp;
			syms[nsym].typ = L_GLOBAL|L_REF;
			syms[nsym].addr = sp->s_value;
			++nsym;
		}
	for (i = 0; i < nfix; ++i) {		/* undefined externals referenced by fixups */
		if ((FIX(i).sp->s_flag&S_DEF) != 0)
			continue;
		for (j = 0; j < nsym; ++j)
			if (syms[j].sp == FIX(i).sp)
				break;
		if (j < nsym)
			continue;
		if (nsym >= nsymcap)
			cfatal("outcoh: symbol table overflow");
		syms[nsym].sp = FIX(i).sp;
		syms[nsym].typ = L_GLOBAL|L_REF;
		syms[nsym].addr = 0;
		++nsym;
	}
	/* SS -- the stack segment, defined by whatever this object is linked into (0 by a
	 * program's crts0.s, 0x3F by the kernel's md.s).  Named only when a frame address
	 * asks for it: an object with no frames must not oblige its link to define it. */
	sssym = -1;
	if (nsfix > 0) {
		sssym = nsym;
		syms[nsym].sp = glookup("SS", 0);
		syms[nsym].typ = L_GLOBAL|L_REF;
		syms[nsym].addr = 0;
		++nsym;
	}

	/* each symbol remembers its table index: the relocations name it by that */
	for (i = 0; i < nsym; ++i)
		syms[i].sp->s_ref = i;

	/* resolve fixups: patch each referencing word.  The relocation records are
	 * written at the end straight from the fixup lists (see putrels()); nothing
	 * collects them, and relsize is what they will come to. */
	relsize = 0;
	for (i = 0; i < nfix; ++i) {
		register SYM	*fs;
		long		val;

		fs = FIX(i).sp;
		/* The fixup was recorded at the OFFSET word of a 2-word segmented operand; the
		 * LR_LONG relocation covers the whole seg:offset pair, so it anchors at the SEGMENT
		 * word (off-2).  patchaddr() fills the offset word, and the segment word too
		 * when the value is negative; ld reads the full long, adds the target
		 * segment's base, and re-derives the segment. */
		if ((fs->s_flag&S_DEF) != 0 && fs->s_seg == SCODE)
			val = fs->s_value + FIX(i).addend;	/* defined function (text) */
		else if ((fs->s_flag&S_DEF) != 0)		/* defined data (any non-code segment) */
			val = textsize + segbase[fs->s_seg] + fs->s_value + FIX(i).addend;
		else					/* undefined external */
			val = FIX(i).addend;
		relsize += ((fs->s_flag&S_DEF) != 0) ? 5 : 7;
		patchaddr(FIX(i).seg, FIX(i).off, val);
	}
	relsize += (long)njfix * 5 + (long)nsfix * 7;
	symsize = (long)nsym * 22;

	/* header (48 bytes): magic, flag LF_32, machine z8001, tbase, 9 section sizes, entry */
	leput(0407); leput(0x10); leput(4); leput(48);
	canput((unsigned long)textsize);	/* ss[0] SHRI */
	canput(0L); canput(0L); canput(0L);	/* ss[1..3] */
	canput((unsigned long)datasize);	/* ss[4] PRVD (file-resident data) */
	canput((unsigned long)bsssize);		/* ss[5] BSSD (reserved, no file bytes) */
	canput(0L);				/* ss[6] DEBUG */
	canput((unsigned long)symsize);		/* ss[7] SYM */
	canput((unsigned long)relsize);		/* ss[8] REL */
	canput(0L);				/* entry */

	for (off = 0; off < olen[SCODE]; ++off)	/* text */
		putc(OB(SCODE, off), ofp);
	for (j = 0; j < NPSEG; ++j) {		/* PRVD image: each segment's bytes, then zero-pad */
		register int	ds;
		ds = dataseg[j];
		for (off = 0; off < olen[ds]; ++off)
			putc(OB(ds, off), ofp);
		for (off = olen[ds]; off < seg[ds].s_dot; ++off)
			putc(0, ofp);			/* reserved tail within an initialized segment */
		if (seg[ds].s_dot & 1)
			putc(0, ofp);			/* word-align to match the padded segbase above */
	}
	/* SBSS emits no file bytes: its size is declared in ss[5] BSSD and the loader zero-fills. */

	for (i = 0; i < nsym; ++i) {		/* symbol table: native ldsym, 22 bytes each */
		register char	*name;
		name = syms[i].sp->s_id;
		for (j = 0; j < 16; ++j)
			putc(j < (int)strlen(name) ? name[j] : 0, ofp);
		leput(syms[i].typ);		/* ls_type at +16 */
		canput(syms[i].addr);		/* ls_addr at +18 */
	}

	putrels(textsize, segbase, sssym);
}

/*
 * One relocation record.  A cc2 symbol reference is a 2-word segmented address
 * operand, so those relocations are LR_LONG (seg:offset).  ld's vtop/base/ptov
 * fills the hardware segment (code -> its segment, data -> its segment); the 0x8000
 * present bit rides through unchanged.  A flat link resolves to segment 0 exactly as
 * the old LR_WORD form did.  The frame-address ones are LR_BYTE against SS: they
 * relocate the segment byte alone, leaving the offset the compiler computed.
 */
static
putrel(addr, width, issym, code, symno)
unsigned long	addr;
{
	if (issym) {
		putc(LR_SYM|width, ofp); putc(0, ofp); putc(0, ofp);
		putc((int)(addr&0xFF), ofp); putc((int)((addr>>8)&0xFF), ofp);
		putc(symno&0xFF, ofp); putc((symno>>8)&0xFF, ofp);
	} else {
		putc(code|width, ofp); putc(0, ofp); putc(0, ofp);
		putc((int)(addr&0xFF), ofp); putc((int)((addr>>8)&0xFF), ofp);
	}
}

/*
 * The record for fixup i, anchored at the segment word of its operand.
 */
static
putfix(i, addr)
unsigned long	addr;
{
	register SYM	*fs;

	fs = FIX(i).sp;
	if ((fs->s_flag&S_DEF) == 0)
		putrel(addr, LR_LONG, 1, 0, fs->s_ref);
	else
		putrel(addr, LR_LONG, 0, fs->s_seg == SCODE ? L_SHRI
			: fs->s_seg == SBSS ? L_BSSD : L_PRVD, 0);
}

/*
 * Write every relocation in ascending address order without ever holding them
 * all: each list is already ascending (a segment's fixups are recorded as its
 * bytes are emitted, and a rewound pass drops its tail), so the three text lists
 * (symbol fixups, promoted JP targets, frame-segment bytes) are merged, and the
 * data segments follow in image order.  ld streams relocations forward per
 * segment, so the order is load-bearing.
 */
static
putrels(textsize, segbase, sssym)
long		textsize, segbase[];
{
	register int	fi, ji, si, j, ds;
	long		fa, ja, sa;

	fi = ji = si = 0;
	for (;;) {
		while (fi < nfix && FIX(fi).seg != SCODE)
			++fi;
		fa = fi < nfix ? FIX(fi).off - 2 : -1;
		ja = ji < njfix ? jfixv[ji].off - 2 : -1;
		sa = si < nsfix ? sfixv[si] : -1;
		if (fa < 0 && ja < 0 && sa < 0)
			break;
		if (fa >= 0 && (ja < 0 || fa <= ja) && (sa < 0 || fa <= sa)) {
			putfix(fi, (unsigned long)fa);
			++fi;
		} else if (ja >= 0 && (sa < 0 || ja <= sa)) {
			putrel((unsigned long)ja, LR_LONG, 0, L_SHRI, 0);
			++ji;
		} else {
			putrel((unsigned long)sa, LR_BYTE, 1, 0, sssym);
			++si;
		}
	}
	for (j = 0; j < NPSEG; ++j) {
		ds = dataseg[j];
		for (fi = 0; fi < nfix; ++fi)
			if (FIX(fi).seg == ds)
				putfix(fi, (unsigned long)(textsize + segbase[ds] + FIX(fi).off - 2));
	}
}
