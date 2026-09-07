/*
 * common/z8001/tyname.c
 * Machine dependent type names, indexed by the machine type codes in
 * h/z8001/mch.h.  The Z8001 codes follow the i8086's (the template this back
 * end was built from), which is NOT the numbering the original 1985 MWC Z8001
 * compiler used -- its cc3 spells the same set in the order
 * S8 U8 S16 U16 S32 U32 PTR F32 F64 PTB BLK FLD8 FLD16 FLD32.  The order here
 * is the one this compiler's own passes agree on; keep it in step with mch.h.
 */

char	*tynames[] = {
	"S8",		/* signed byte */
	"U8",		/* unsigned byte */
	"S16",		/* signed word */
	"U16",		/* unsigned word */
	"S32",		/* signed long */
	"U32",		/* unsigned long */
	"F32",		/* short float */
	"F64",		/* long float */
	"Blk",		/* block of bytes */
	"Fld8",		/* bit field, byte wide */
	"Fld16",	/* bit field, word wide */
	"Lptr",		/* large (seg:off) pointer */
	"Lptb",		/* large pointer to BLK */
	"Sptr",		/* small (offset only) pointer */
	"Sptb"		/* small pointer to BLK */
};

/* end of common/z8001/tyname.c */
