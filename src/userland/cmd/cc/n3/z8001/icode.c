/*
 * n3/z8001/icode.c
 * The machine dependent part of the intermediate file printer: it reads and
 * prints encoded machine instructions and their operands.  Segmented Z8001.
 * Template: n3/i8086/icode.c.
 *
 * This file READS the same i1 CODE stream that n2 reads, so every decision here
 * must match n2/z8001: the row order and operand counts of ins[] below are
 * n2/z8001/optab.c's, and the address-field decode in iafield() is
 * n2/z8001/afield.c's getfield().  A disagreement is not a cosmetic one -- the
 * stream is self-delimiting only if the reader knows how many words each field
 * occupies, so one wrong operand count desynchronizes everything after it.
 */
#ifdef	vax
#include "INC$LIB:cc3.h"
#else
#include "cc3.h"
#endif
#include "pseudops.h"		/* ZBYTE..ZGPTR, ZJREL */

#define	CC_ALWAYS	8	/* Z8000 condition code T (always) */

/*
 * Instruction names, operand counts, and flags.
 * Ordered by generated/opcode.h and thus parallel to n2/z8001/optab.c;
 * modifications to any of these require modifications to all of them.
 * (tests/cc3tab.sh checks that mechanically.)
 * Rows 0..161 are the real Z8001 opcodes, 162..191 are unused, and 192..195 are
 * the pseudops.h data directives; the array covers the whole 0..0xC3 range so a
 * corrupt opcode byte indexes in bounds and prints as "bad opcode" instead of
 * walking off the table.
 */
static struct ins {
	char *i_name;
	char  i_size;
	char  i_flag;
} ins[0xC4] = {
	/* 0   ZADC         */	"adc",         2,	0,
	/* 1   ZADCB        */	"adcb",        2,	OP_BYTE,
	/* 2   ZADD         */	"add",         2,	0,
	/* 3   ZADDB        */	"addb",        2,	OP_BYTE,
	/* 4   ZADDL        */	"addl",        2,	OP_DWORD,
	/* 5   ZAND         */	"and",         2,	0,
	/* 6   ZANDB        */	"andb",        2,	OP_BYTE,
	/* 7   ZBIT         */	"bit",         2,	0,
	/* 8   ZBITB        */	"bitb",        2,	OP_BYTE,
	/* 9   ZCALL        */	"call",        1,	OP_DWORD,
	/* 10  ZCALR        */	"calr",        1,	OP_JUMP,
	/* 11  ZCLR         */	"clr",         1,	0,
	/* 12  ZCLRB        */	"clrb",        1,	OP_BYTE,
	/* 13  ZCOM         */	"com",         1,	0,
	/* 14  ZCOMB        */	"comb",        1,	OP_BYTE,
	/* 15  ZCOMFLG      */	"comflg",      1,	0,
	/* 16  ZCP          */	"cp",          2,	0,
	/* 17  ZCPB         */	"cpb",         2,	OP_BYTE,
	/* 18  ZCPD         */	"cpd",         2,	OP_DWORD,
	/* 19  ZCPDB        */	"cpdb",        2,	OP_BYTE,
	/* 20  ZCPDR        */	"cpdr",        2,	OP_DWORD,
	/* 21  ZCPDRB       */	"cpdrb",       2,	OP_BYTE,
	/* 22  ZCPI         */	"cpi",         2,	OP_DWORD,
	/* 23  ZCPIB        */	"cpib",        2,	OP_BYTE,
	/* 24  ZCPIR        */	"cpir",        2,	OP_DWORD,
	/* 25  ZCPIRB       */	"cpirb",       2,	OP_BYTE,
	/* 26  ZCPL         */	"cpl",         2,	OP_DWORD,
	/* 27  ZCPSD        */	"cpsd",        2,	OP_DWORD,
	/* 28  ZCPSDB       */	"cpsdb",       2,	OP_BYTE,
	/* 29  ZCPSDR       */	"cpsdr",       2,	OP_DWORD,
	/* 30  ZCPSDRB      */	"cpsdrb",      2,	OP_BYTE,
	/* 31  ZCPSI        */	"cpsi",        2,	OP_DWORD,
	/* 32  ZCPSIB       */	"cpsib",       2,	OP_BYTE,
	/* 33  ZCPSIR       */	"cpsir",       2,	OP_DWORD,
	/* 34  ZCPSIRB      */	"cpsirb",      2,	OP_BYTE,
	/* 35  ZDAB         */	"dab",         1,	0,
	/* 36  ZDEC         */	"dec",         2,	0,
	/* 37  ZDECB        */	"decb",        2,	OP_BYTE,
	/* 38  ZDI          */	"di",          0,	0,
	/* 39  ZDIV         */	"div",         2,	OP_DWORD,
	/* 40  ZDIVL        */	"divl",        2,	OP_DWORD,
	/* 41  ZDJNZ        */	"djnz",        2,	OP_JUMP,
	/* 42  ZEI          */	"ei",          0,	0,
	/* 43  ZEPU         */	"epu",         0,	0,
	/* 44  ZEX          */	"ex",          2,	0,
	/* 45  ZEXB         */	"exb",         2,	OP_BYTE,
	/* 46  ZEXTS        */	"exts",        1,	OP_DWORD,
	/* 47  ZEXTSB       */	"extsb",       1,	OP_BYTE,
	/* 48  ZEXTSL       */	"extsl",       1,	OP_DWORD,
	/* 49  ZHALT        */	"halt",        0,	0,
	/* 50  ZILLEGAL     */	"illegal",     1,	OP_DWORD,
	/* 51  ZIN          */	"in",          2,	0,
	/* 52  ZINB         */	"inb",         2,	OP_BYTE,
	/* 53  ZINC         */	"inc",         2,	0,
	/* 54  ZINCB        */	"incb",        2,	OP_BYTE,
	/* 55  ZINDR        */	"indr",        2,	OP_DWORD,
	/* 56  ZINDRB       */	"indrb",       2,	OP_BYTE,
	/* 57  ZINIR        */	"inir",        2,	OP_DWORD,
	/* 58  ZINIRB       */	"inirb",       2,	OP_BYTE,
	/* 59  ZIRET        */	"iret",        0,	0,
	/* 60  ZJP          */	"jp",          1,	0,
	/* 61  ZJR          */	"jr",          1,	OP_JUMP,
	/* 62  ZLD          */	"ld",          2,	0,
	/* 63  ZLDA         */	"lda",         2,	OP_DWORD,
	/* 64  ZLDAR        */	"ldar",        2,	0,
	/* 65  ZLDB         */	"ldb",         2,	OP_BYTE,
	/* 66  ZLDCTL       */	"ldctl",       2,	OP_DWORD,
	/* 67  ZLDCTLBFLAGSRB */	"ldctlb",     1,	OP_BYTE,
	/* 68  ZLDCTLBRBFLAGS */	"ldctlb",     1,	OP_BYTE,
	/* 69  ZLDDR        */	"lddr",        3,	OP_DWORD,
	/* 70  ZLDDRB       */	"lddrb",       3,	OP_BYTE,
	/* 71  ZLDIR        */	"ldir",        3,	OP_DWORD,
	/* 72  ZLDIRB       */	"ldirb",       3,	OP_BYTE,
	/* 73  ZLDK         */	"ldk",         2,	0,
	/* 74  ZLDL         */	"ldl",         2,	OP_DWORD,
	/* 75  ZLDM         */	"ldm",         2,	0,
	/* 76  ZLDPS        */	"ldps",        1,	0,
	/* 77  ZMBIT        */	"mbit",        0,	0,
	/* 78  ZMREQ        */	"mreq",        0,	0,
	/* 79  ZMRES        */	"mres",        0,	0,
	/* 80  ZMSET        */	"mset",        0,	0,
	/* 81  ZMULT        */	"mult",        2,	OP_DWORD,
	/* 82  ZMULTL       */	"multl",       2,	OP_DWORD,
	/* 83  ZNEG         */	"neg",         1,	0,
	/* 84  ZNEGB        */	"negb",        1,	OP_BYTE,
	/* 85  ZNOP         */	"nop",         0,	0,
	/* 86  ZOR          */	"or",          2,	0,
	/* 87  ZORB         */	"orb",         2,	OP_BYTE,
	/* 88  ZOTDR        */	"otdr",        2,	OP_DWORD,
	/* 89  ZOTDRB       */	"otdrb",       2,	OP_BYTE,
	/* 90  ZOTIR        */	"otir",        2,	OP_DWORD,
	/* 91  ZOTIRB       */	"otirb",       2,	OP_BYTE,
	/* 92  ZOUT         */	"out",         2,	0,
	/* 93  ZOUTB        */	"outb",        2,	OP_BYTE,
	/* 94  ZPOP         */	"pop",         1,	0,
	/* 95  ZPOPL        */	"popl",        1,	OP_DWORD,
	/* 96  ZPUSH        */	"push",        1,	0,
	/* 97  ZPUSHL       */	"pushl",       1,	OP_DWORD,
	/* 98  ZRES         */	"res",         2,	0,
	/* 99  ZRESB        */	"resb",        2,	OP_BYTE,
	/* 100 ZRESFLG      */	"resflg",      1,	0,
	/* 101 ZRET         */	"ret",         1,	0,
	/* 102 ZRL          */	"rl",          1,	OP_DWORD,
	/* 103 ZRLB         */	"rlb",         1,	OP_BYTE,
	/* 104 ZRLC         */	"rlc",         1,	0,
	/* 105 ZRLCB        */	"rlcb",        1,	OP_BYTE,
	/* 106 ZRLDB        */	"rldb",        1,	0,
	/* 107 ZRR          */	"rr",          1,	0,
	/* 108 ZRRB         */	"rrb",         1,	OP_BYTE,
	/* 109 ZRRC         */	"rrc",         1,	0,
	/* 110 ZRRCB        */	"rrcb",        1,	OP_BYTE,
	/* 111 ZRRDB        */	"rrdb",        1,	0,
	/* 112 ZSBC         */	"sbc",         2,	0,
	/* 113 ZSBCB        */	"sbcb",        2,	OP_BYTE,
	/* 114 ZSC          */	"sc",          1,	0,
	/* 115 ZSDA         */	"sda",         2,	0,
	/* 116 ZSDAB        */	"sdab",        2,	OP_BYTE,
	/* 117 ZSDAL        */	"sdal",        2,	OP_DWORD,
	/* 118 ZSDL         */	"sdl",         2,	OP_DWORD,
	/* 119 ZSDLB        */	"sdlb",        2,	OP_BYTE,
	/* 120 ZSDLL        */	"sdll",        2,	OP_DWORD,
	/* 121 ZSET         */	"set",         2,	0,
	/* 122 ZSETB        */	"setb",        2,	OP_BYTE,
	/* 123 ZSETFLG      */	"setflg",      1,	0,
	/* 124 ZSIN         */	"sin",         2,	0,
	/* 125 ZSINB        */	"sinb",        2,	OP_BYTE,
	/* 126 ZSINDR       */	"sindr",       2,	OP_DWORD,
	/* 127 ZSINDRB      */	"sindrb",      2,	OP_BYTE,
	/* 128 ZSINIR       */	"sinir",       2,	OP_DWORD,
	/* 129 ZSINIRB      */	"sinirb",      2,	OP_BYTE,
	/* 130 ZSLA         */	"sla",         2,	0,
	/* 131 ZSLAB        */	"slab",        2,	OP_BYTE,
	/* 132 ZSLAL        */	"slal",        2,	OP_DWORD,
	/* 133 ZSLL         */	"sll",         2,	OP_DWORD,
	/* 134 ZSLLB        */	"sllb",        2,	OP_BYTE,
	/* 135 ZSLLL        */	"slll",        2,	OP_DWORD,
	/* 136 ZSOTDR       */	"sotdr",       2,	OP_DWORD,
	/* 137 ZSOTDRB      */	"sotdrb",      2,	OP_BYTE,
	/* 138 ZSOTIR       */	"sotir",       2,	OP_DWORD,
	/* 139 ZSOTIRB      */	"sotirb",      2,	OP_BYTE,
	/* 140 ZSOUT        */	"sout",        2,	0,
	/* 141 ZSOUTB       */	"soutb",       2,	OP_BYTE,
	/* 142 ZSUB         */	"sub",         2,	0,
	/* 143 ZSUBB        */	"subb",        2,	OP_BYTE,
	/* 144 ZSUBL        */	"subl",        2,	OP_DWORD,
	/* 145 ZTCC         */	"tcc",         1,	0,
	/* 146 ZTCCB        */	"tccb",        1,	OP_BYTE,
	/* 147 ZTEST        */	"test",        1,	0,
	/* 148 ZTESTB       */	"testb",       1,	OP_BYTE,
	/* 149 ZTESTL       */	"testl",       1,	OP_DWORD,
	/* 150 ZTRDB        */	"trdb",        2,	OP_DWORD,
	/* 151 ZTRDRB       */	"trdrb",       2,	OP_DWORD,
	/* 152 ZTRIB        */	"trib",        2,	OP_DWORD,
	/* 153 ZTRIRB       */	"trirb",       2,	OP_DWORD,
	/* 154 ZTRTDB       */	"trtdb",       2,	OP_DWORD,
	/* 155 ZTRTDRB      */	"trtdrb",      2,	OP_DWORD,
	/* 156 ZTRTIB       */	"trtib",       2,	OP_DWORD,
	/* 157 ZTRTIRB      */	"trtirb",      2,	OP_DWORD,
	/* 158 ZTSET        */	"tset",        1,	0,
	/* 159 ZTSETB       */	"tsetb",       1,	OP_BYTE,
	/* 160 ZXOR         */	"xor",         2,	0,
	/* 161 ZXORB        */	"xorb",        2,	OP_BYTE,
	/* 162 --           */	0,             0,	0,
	/* 163 --           */	0,             0,	0,
	/* 164 --           */	0,             0,	0,
	/* 165 --           */	0,             0,	0,
	/* 166 --           */	0,             0,	0,
	/* 167 --           */	0,             0,	0,
	/* 168 --           */	0,             0,	0,
	/* 169 --           */	0,             0,	0,
	/* 170 --           */	0,             0,	0,
	/* 171 --           */	0,             0,	0,
	/* 172 --           */	0,             0,	0,
	/* 173 --           */	0,             0,	0,
	/* 174 --           */	0,             0,	0,
	/* 175 --           */	0,             0,	0,
	/* 176 --           */	0,             0,	0,
	/* 177 --           */	0,             0,	0,
	/* 178 --           */	0,             0,	0,
	/* 179 --           */	0,             0,	0,
	/* 180 --           */	0,             0,	0,
	/* 181 --           */	0,             0,	0,
	/* 182 --           */	0,             0,	0,
	/* 183 --           */	0,             0,	0,
	/* 184 --           */	0,             0,	0,
	/* 185 --           */	0,             0,	0,
	/* 186 --           */	0,             0,	0,
	/* 187 --           */	0,             0,	0,
	/* 188 --           */	0,             0,	0,
	/* 189 --           */	0,             0,	0,
	/* 190 --           */	0,             0,	0,
	/* 191 --           */	0,             0,	0,
	/* 192 ZBYTE        */	".byte",       1,	OP_DD|OP_NPTR,
	/* 193 ZWORD        */	".word",       1,	OP_DD|OP_NPTR,
	/* 194 ZLPTR        */	".long",       1,	OP_DD|OP_NPTR,
	/* 195 ZGPTR        */	".long",       1,	OP_DD|OP_NPTR,
};

/*
 * The Z8000 condition codes, indexed by the 4-bit cc field.  These are the
 * spellings as-z8001 accepts (src/as/z8001/pst.c), which are also the ones the
 * original Z8001 cc3 printed -- including "un" for code 8, the unconditional
 * branch cc1 emits for a plain jump and for JP.  Code 0 (F, never) has no
 * assembler name because nothing generates it; it prints as a bare "jr".
 */
static	char	*ccnames[16] = {
	"",	"lt",	"le",	"ule",	"ov",	"mi",	"eq",	"ult",
	"un",	"ge",	"gt",	"ugt",	"nov",	"pl",	"ne",	"uge"
};

/*
 * Print a branch to a local label.  cc1 writes the target as a label-direct
 * address field (A_LID|A_DIR, then the label number) -- the same shape
 * n2/z8001/getcod.c consumes for its span-dependent JUMP node.
 */
static
ijump(cc)
{
	if (iget() != (A_LID|A_DIR))
		cbotch("jump target");
	fprintf(ofp, "\tjr");
	if (ccnames[cc][0] != 0)
		fprintf(ofp, "\t%s,", ccnames[cc]);
	else
		bput('\t');
	fprintf(ofp, "L" I_FMT "\n", iget());
}

/*
 * Handle code nodes.
 */
icode()
{
	register int op, i, n;
	register struct ins *ip;

	op = bget();
	/*
	 * Two i1 conventions that are NOT table rows (n2/z8001/getcod.c reads
	 * them the same way): the 0xD0..0xDF conditional relative-jump band, and
	 * JP, which n2 also turns into a relaxable relative jump.
	 */
	if (op >= ZJREL && op <= (ZJREL|0xF)) {
		ijump(op & 0xF);
		return;
	}
	if (op == ZJP) {
		ijump(CC_ALWAYS);
		return;
	}
	if (op < 0 || op >= (int)(sizeof(ins)/sizeof(ins[0]))
	 || ins[op].i_name == NULL) {
		fprintf(ofp, "\tbad opcode %d\n", op);
		cbotch("bad code opcode %d", op);
	}
	ip = &ins[op];
	bput('\t');
	fprintf(ofp, "%s", ip->i_name);
	n = ip->i_size;
	for (i=0; i < n; i += 1) {
		if (i == 0)
			bput('\t');
		else
			fprintf(ofp, ", ");
		iafield(ip);
	}
	bput('\n');
}

/*
 * Register names by hardware encoding, for the register codes that live in the
 * A_REGM nibble of an address mode.  These are NOT the compiler's internal
 * register numbers (mch.h R0/RR0/RH0...) that a REG tree leaf carries and that
 * regnames[] spells; by the time cc1 has written an address field the number in
 * it is what the instruction encodes.  A pair or a quad is named by its low
 * word register, and a byte register by the grouped encoding: 0..7 = RHn,
 * 8..15 = RLn.
 */
static
iareg(regm, ip)
register struct ins	*ip;
{
	if ((ip->i_flag&OP_DWORD) != 0)
		fprintf(ofp, "rr%d", regm);
	else
		fprintf(ofp, "r%d", regm);
}

/*
 * Print the symbol (and/or displacement) part of an address field.
 * `id' holds the symbol name already read by iafield().
 */
static
iaddr(imode, offs)
ADDRESS	offs;
{
	if ((imode&(A_LID|A_GID)) != 0) {
		fprintf(ofp, "%s", id);
		if ((SIGNEDADDRESS)offs > 0)
			bput('+');
		else if ((SIGNEDADDRESS)offs == 0)
			return;
	}
	fprintf(ofp, "%ld", (long)(SIGNEDADDRESS)offs);
}

/*
 * Read and print an address field.  The style of the address is determined by
 * flag bits that hide in the address mode; those bits are cleared away when the
 * mode is actually stored in the afield.  The read sequence -- and it is a
 * read, so it has to be exact -- is n2/z8001/afield.c getfield()'s:
 *
 *	mode word
 *	A_IMML:      two words, high then low (32-bit immediate)
 *	A_OFFS with a symbol: two words, high then low (32-bit displacement)
 *	A_OFFS bare:          one word, signed
 *	A_LID:  label number   A_GID: symbol name
 */
iafield(ip)
register struct ins	*ip;
{
	register int	imode;
	register int	mode;
	register int	regm;
	ADDRESS		offs;

	imode = iget();
	offs = 0;
	if ((imode&A_AMOD) == A_IMML) {
		register unsigned int	hi, lo;
		hi = iget() & 0xFFFF;
		lo = iget() & 0xFFFF;
		offs = ((unsigned long)hi << 16) | lo;
		fprintf(ofp, "$%ld", (long)offs);
		return;
	}
	if ((imode&A_OFFS) != 0) {
		if ((imode&(A_LID|A_GID)) != 0) {
			register long	v;
			v  = (long)(iget() & 0xFFFF) << 16;
			v |= iget() & 0xFFFF;
			if ((v & 0x80000000L) != 0)
				v |= ~0xFFFFFFFFL;
			offs = v;
		} else
			offs = iget();
	}
	if ((imode&A_LID) != 0)
		sprintf(id, "L" I_FMT, iget());
	else if ((imode&A_GID) != 0)
		sget(id, NCSYMB);
	mode = imode&A_AMOD;
	regm = imode&A_REGM;
	switch (mode) {

	case A_WR:
		iareg(regm, ip);
		break;

	case A_BR:
		if (regm < 8)
			fprintf(ofp, "rh%d", regm);
		else
			fprintf(ofp, "rl%d", regm-8);
		break;

	case A_IMM:
		bput('$');
		iaddr(imode, offs);
		break;

	case A_IR:
		/* @RRn, or RRn(disp) -- the base-address form -- when cc1 folded
		 * a displacement onto the pointer in the pair. */
		if (offs != 0)
			fprintf(ofp, "rr%d(%ld)", regm, (long)(SIGNEDADDRESS)offs);
		else
			fprintf(ofp, "@rr%d", regm);
		break;

	case A_DIR:
		if ((imode&A_CS) != 0)
			fprintf(ofp, "code:");	/* address is in the code space */
		iaddr(imode, offs);
		break;

	case A_X:
		iaddr(imode, offs);
		fprintf(ofp, "(r%d)", regm);
		break;

	default:
		cbotch("iafield, mode=%d", mode>>4);
	}
}

/* end of n3/z8001/icode.c */
