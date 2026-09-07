/*
 * n1/z8001/cc1mch.h
 * Machine-specific macros, types and definitions used ONLY by the cc1 code
 * generator, for the segmented Z8001.
 *
 * RE-BASED on h/i8086/cc1mch.h (cc-multitarget-3.2.1b) -- the 16-bit, segmented,
 * SMALL/LARGE-model analog -- NOT the 32-bit-flat i386.
 * The i8086 supplies the 16-bit machinery the Z8001 also needs
 * and the i386 lacks: KLP/KSP pointer kinds (kept distinct from long), the
 * high/low half-constant tree flags (T_UHS/T_LHS/T_UHC/T_LHC) for splitting a
 * 32-bit long into two 16-bit halves, upper()/lower(), and LONGREL.
 *
 * mch.h is included first (h/cc1.h) and owns the Z8001 register MODEL (R0..R15,
 * pairs, byte regs, pseudos), the A_* modes, PREGSET/BREG, and the ABI sets
 * SWREG/FNUSED/NOFREE + GIDFMT/LIDFMT/CONFMT/mapssize/DOWN/ICALLS -- so they are
 * NOT redefined here.
 *
 * Z8001 vs i8086: the Z8001 has NO segment-override registers (no ES/DS); the
 * 7-bit segment travels inside the address (mch.h A_SEG / companion AFIELD), so
 * the i8086 segment-register pairs (esax/dsbx...) have no analog and the ZLDES
 * "load ES" opcode mapping is dropped. T_ACS/T_ADS map to the Z8000's separate
 * code/data address spaces.
 */

/* Type definitions. (PREGSET lives in mch.h.) */
typedef	char	COST;		/* Cost of evaluation			*/
typedef	char	TYPE;		/* Machine type				*/
typedef	long	FLAG;		/* Flags (must hold all T_* below)	*/
typedef	char	REGNAME;	/* Register name			*/
typedef	int	TYPESET;	/* Set of TYPE				*/
typedef	char	PHYSREG;	/* Physical register name		*/
typedef	unsigned long PATFLAG;	/* Pattern flags, at least 16 bits	*/
typedef	int	KIND;		/* Register kind			*/
typedef	int	MASK;		/* Field masks				*/
typedef	char	INDEX;		/* Index type				*/

/* Manifest constants. */
#define	BITS	0		/* n'th-bit code not needed		*/
#define	LONGREL	1		/* long relational code IS needed (16-bit ALU) */
#define	MBLARG	MFNARG		/* Block argument context		*/
#define	MBLREG	ANYR		/* Block argument register		*/
#define	DVALIS	0		/* Index into dval for DVAL sign	*/
#define	DVALMS	0200		/* Bit to flip for DVAL sign		*/
#define	NBPCH	8		/* # of bits in a char (out.c)		*/
#define	NSWITCH	4		/* # cases in a switch coded as conditionals */

/* Split a 32-bit long into its 16-bit halves (big-endian: hi word first). */
#define	upper(n)	((ival_t)((unsigned long)(n) >> 16))
#define	lower(n)	((ival_t)(n))

/* Macros. */
#define	isblkp(t)	((t)==LPTB || (t)==SPTB)	/* pointer-to-BLK leaf	*/
#if	DECVAX
#define	poolseg(op)	((op!=DCON) ? SLINK : SDATA)
#else
#define	poolseg(op)	((notvariant(VRAM)||(op!=DCON)) ? SLINK : SDATA)
#endif

#if	!TINY
/* Debug printout macros (explained in snap1.c). */
#define	isnap(x)	printf(" %d", (x))
#define	lsnap(x)	printf(" %ld", (x))
#define	csnap(x)	((x)!=0?printf(" cost=%d", (x)):0)
#define	fsnap(x)	((x)!=0?printf(" flag=%lx", (x)):0)
#define	mdlsnap(x)	snaptype((x), "Bad leaf")
#define	mdosnap(x)	snaptype((x), "Bad op")
#endif

/*
 * Tree flags (i8086 layout -- kept identical so the i8086-derived backend files
 * and .t tables match). Low bits = constant/half-constant range tests; the
 * half-constant flags split a 32-bit long for 16-bit codegen. T_ACS/T_ADS are
 * the Z8000 code-space / data-space address distinction. Top 4 bits = the MI
 * control flags (TREG/LV/MMX/INDIR).
 */
#define	T_0	0x00000001L	/* constant 0			*/
#define	T_1	0x00000002L	/* constant 1			*/
#define	T_2	0x00000004L	/* constant 2 (word-size scale)	*/
#define	T_BYTE	0x00000008L	/* [-128 .. 127]		*/
#define	T_ICN	0x00000010L	/* ICON				*/
#define	T_LCN	0x00000020L	/* LCON				*/
#define	T_UHS	0x00000040L	/* LCON, 0xFFFF.... (upper-half set)	*/
#define	T_LHS	0x00000080L	/* LCON, 0x....FFFF (lower-half set)	*/
#define	T_UHC	0x00000100L	/* LCON, 0x0000.... (upper-half clear)	*/
#define	T_LHC	0x00000200L	/* LCON, 0x....0000 (lower-half clear)	*/
#define	T_DCN	0x00000400L	/* DCON				*/
#define	T_ACS	0x00000800L	/* ADDR, code address space	*/
#define	T_ADS	0x00001000L	/* ADDR, data address space	*/
#define	T_RREG	0x00002000L	/* REG, rvalue			*/
#define	T_LREG	0x00004000L	/* REG, lvalue			*/
#define	T_SREG	0x00008000L	/* REG, stack (FP/SP relative)	*/
#define	T_LEA	0x00010000L	/* computable by LDA		*/
#define	T_LSS	0x00020000L	/* LDA off the stack segment	*/
#define	T_DIR	0x00040000L	/* direct address (DA)		*/
#define	T_OFS	0x00080000L	/* offset present		*/

#define	T_TREG	0x80000000L	/* need a temporary register	*/
#define	T_LV	0x40000000L	/* lvalue context		*/
#define	T_MMX	0x20000000L	/* must match shape exactly	*/
#define	T_INDIR	0x10000000L	/* fake indirect flag		*/
#define	T_NBH	0x00200000L	/* REG with NO BYTE HALF -- a word register R8..R15,
				 * which the Z8000 cannot name as RHn/RLn (reg[].r_lohalf
				 * is -1).  A byte rule that dials the operand's [LO] half
				 * must be steered away from such a register */
#define	T_FOLDOFS 0x00100000L	/* modoper marks a far-pointer field deref that is
				 * a LOAD/STORE (not an ALU/compare operand), so findoffs
				 * FOLDS its constant offset into the BA displacement
				 * instead of materializing the element address */

#define	T_NUM	(T_ICN|T_LCN)
#define	T_CON	(T_NUM|T_ACS|T_ADS)
#define	T_IMM	(T_CON|T_DCN)
#define	T_REG	(T_RREG|T_LREG|T_SREG)
#define	T_ADR	(T_DIR|T_RREG|T_LREG|T_SREG)
#define	T_LEAF	(T_ADR|T_IMM|T_OFS|T_LEA|T_LSS)
#define	T_EASY	(T_DIR|T_IMM|T_OFS|T_LREG|T_RREG|T_SREG)
#define	T_NLEAF	(T_IMM|T_ADR)

/*
 * Z8000 condition codes (the 4-bit cc field of JR/JP/CALR/RET). The Z8000 has
 * ONE conditional jump + a cc, so table1.c's optab relation rows hold these and
 * the coder emits [ZJR]/[ZJP]+cc.
 */
#define	CC_F	0x0	/* never	*/
#define	CC_LT	0x1	/* signed <	*/
#define	CC_LE	0x2	/* signed <=	*/
#define	CC_ULE	0x3	/* unsigned <=	*/
#define	CC_OV	0x4	/* overflow	*/
#define	CC_MI	0x5	/* minus	*/
#define	CC_EQ	0x6	/* == (Z)	*/
#define	CC_ULT	0x7	/* unsigned <	*/
#define	CC_T	0x8	/* always	*/
#define	CC_GE	0x9	/* signed >=	*/
#define	CC_GT	0xA	/* signed >	*/
#define	CC_UGT	0xB	/* unsigned >	*/
#define	CC_NOV	0xC	/* no overflow	*/
#define	CC_PL	0xD	/* plus		*/
#define	CC_NE	0xE	/* != (NZ)	*/
#define	CC_UGE	0xF	/* unsigned >=	*/

/* Relational-jump opcode. The Z8000 has ONE conditional jump + a 4-bit cc, so
 * the optab relation rows hold ZJREL|cc (one opcode byte per relation). The
 * coder emits it like any jump ([REL0] = optab[rel][0], or gencbr); n2 maps the
 * 0xD0..0xDF range to a relative conditional jump (JR cc) to a label. */
#define	ZJREL	0xD0	/* base; OR in a CC_* condition code	*/

/*
 * Type-testing macros over tinfo[] (table1.c).
 *   01 long  02 word  04 unsigned  010 float  020 int  040 byte
 *   0100 sized  0200 pointer
 */
#define	islong(t)	((tinfo[t]&01)  != 0)
#define	isword(t)	((tinfo[t]&02)  != 0)
#define	isworl(t)	((tinfo[t]&03)  != 0)
#define	isuns(t)	((tinfo[t]&04)  != 0)
#define	isflt(t)	((tinfo[t]&010) != 0)
#define	isint(t)	((tinfo[t]&020) != 0)
#define	isbyte(t)	((tinfo[t]&040) != 0)
#define	isworb(t)	((tinfo[t]&042) != 0)
#define	issized(t)	((tinfo[t]&0100)!= 0)
#define	ispoint(t)	((tinfo[t]&0200)!= 0)

/*
 * Machine-dependent pattern flags (must not overlap cc1.h's). Z8001 fp is the
 * optional Z8070 EPU (EPA templates) or the Extended-Instruction-trap emulator.
 */
#define	PEPU		0x10000L	/* Z8070 EPU (EPA) instructions	*/
#define	PEMUFP		0x20000L	/* soft-fp (Ext-Instr trap) calls */
#define	MDPFLAGS	0x30000L	/* machine-dependent pattern flags */

/*
 * Register kinds (allocator + reg[] in table1.c). KLP/KSP (large/small pointer)
 * are kept DISTINCT from KL: a segmented far pointer (seg:offset) is allocated
 * differently from a plain 32-bit long, exactly as on the i8086.
 */
#define	KB	001		/* byte  (RHn/RLn, R0..R7)		*/
#define	KW	002		/* word  (Rn)				*/
#define	KL	004		/* long  (RRn pair)			*/
#define	KD	010		/* double (RQn quad)			*/
#define	KLP	020		/* large pointer (segmented, RRn pair)	*/
#define	KSP	040		/* small pointer (near, Rn word)	*/

#define	KWB	(KW|KB)		/* word or byte				*/

/* Convenience operand constants for hand-written code sequences (calls,
 * prologue). A_WR is the word-register addressing mode (mch.h); the ABI
 * registers are R15=SP, R13=FP, R0/R1=scratch/return. */
#define	A_RSP	(A_WR|R15)	/* stack pointer operand		*/
#define	A_RFP	(A_WR|R13)	/* frame pointer operand		*/
#define	A_R0	(A_WR|R0)	/* scratch				*/
#define	A_R1	(A_WR|R1)	/* int return / scratch			*/

/* hihalf()/lohalf() (the hi/lo word of a register pair) are machine-INDEPENDENT
 * functions in n1/reg0.c. Z8001 has no x87-style FPAC memory register, so FPAC
 * is an impossible reg number (genadr's FPAC-memory branch is then dead). */
#define	FPAC		(-1)

/*
 * Machine-dependent output escapes (out.c, pool.c). GIDFMT/LIDFMT/CONFMT and
 * mapssize come from mch.h. The Z8001 has no ES-load (ZLDES) and no x87
 * top-of-stack / star addressing, so these are identities / no-ops.
 */
/*
 * Table conditional jumps. The Z8000 has ONE conditional jump and a 4-bit cc, so
 * a .t rule cannot name `JR PL' as an opcode: the encoded form ZJREL|cc lands
 * above M_ORG, where out.c reads a macro byte as an operand escape. Each
 * condition a table body needs therefore gets an opcode INDEX in the free band
 * between the last real opcode (NZOPCODE) and M_ORG, and mapcode turns it into
 * ZJREL|cc on the way out; from there it is the byte gencbr emits and n2's
 * 0xD0..0xDF jump band decodes. The target is [LAB0]/[LAB1], defined by
 * [DLAB0]/[DLAB1] in the same rule.
 */
#define	ZJRPL	(NZOPCODE+0)	/* JR PL,lab  -- sign clear	*/
#define	ZJRULT	(NZOPCODE+1)	/* JR ULT,lab -- unsigned <	*/

#define	mapcode(c, tp)	((c) == ZJRPL  ? (ZJREL|CC_PL)  : \
			 (c) == ZJRULT ? (ZJREL|CC_ULT) : (c))
#define	mappfx(tp, opv, pfx, npfxp)	cbotch("mappfx")	/* unused	*/
#define	gentos(x,y)			/* no top-of-stack required	*/
#define	genstar(x,y,z,zz)		/* no star address required	*/
#define	getstar(tp,nse,npfx,pfx)	/* no star address required	*/
#if	ONLYSMALL
#define	iptrtype()	SPTR		/* non-segmented build		*/
#else
#define	iptrtype()	(isvariant(VSMALL) ? SPTR : LPTR)
#endif

/* Externals. */
#if	!YATC
extern	char	tinfo[];	/* n1/z8001/table1.c	*/
extern	PREGSET	regbusy;	/* n1/z8001/gen1.c	*/
extern	PREGSET	maxbusy;
extern	PREGSET	curbusy;
extern	PREGSET	curxreg;
extern	int	blkflab;	/* n1/z8001/mtree2.c	*/
#endif

/* end of n1/z8001/cc1mch.h */
