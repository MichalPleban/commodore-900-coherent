/ prefac.f - Z8001 type-set + relop-set macros for the n1 selection tables.
/ MUST be the first file fed to tabgen (n1/z8001/Makefile).
/ Type-flag bits (FS16/FU16/...) from h/z8001/mch.h.
/
/ Z8001 is SEGMENTED: the default pointer is LPTR (2-word seg:offset), so pointers
/ group with the 32-bit/LONG class, NOT with WORD (unlike the i8086 ONLYSMALL
/ build where SPTR rode in WORD). Near (SPTR) pointers exist only for an explicit
/ non-segmented data model.

/ ---- type-sets (unions of the per-machine-type flag bits) ----
#define	WORD	(FS16|FU16)		/ 16-bit integer
#define	UWORD	(FU16)			/ unsigned word
#define	INT	(FS16|FU16)		/ C int (== word on Z8001)
#define	BYTE	(FS8|FU8)		/ 8-bit
#define	LONG	(FS32|FU32)		/ 32-bit integer
#define	FLT	(FF32)		/ float (4 bytes, RR pair -- like LONG)
#define	DBL	(FF64)			/ double (8 bytes, RQ quad -- pinned to RQ0)
#define	LPTX	(FLPTR|FLPTB)		/ segmented (far) pointer
#define	SPTX	(FSPTR|FSPTB)		/ near (1-word offset) pointer
#define	PTR	(FLPTR|FLPTB|FSPTR|FSPTB)
#define	NFLT	(BYTE|WORD|LONG|LPTX)	/ everything that is not float
#define	ANYT	(NFLT|FF32|FF64)

/ ---- relop context-flag sets (which compare context a .t rule serves) ----
#define	PREL	(PEQ|PNE|PGT|PGE|PLT|PLE|PUGT|PUGE|PULT|PULE)
#define	PSREL	(PGT|PGE|PLT|PLE)		/ signed relations
#define	PUREL	(PUGT|PUGE|PULT|PULE)		/ unsigned relations
#define	PEREL	(PEQ|PNE)			/ equality
#define	PNEREL	(PGT|PGE|PLT|PLE|PUGT|PUGE|PULT|PULE)	/ non-equality
