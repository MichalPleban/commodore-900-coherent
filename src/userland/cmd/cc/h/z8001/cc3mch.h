/*
 * h/z8001/cc3mch.h
 * C compiler intermediate file interpreter (cc3).
 * Machine dependent defines.  Segmented Z8001.
 * Template: h/i8086/cc3mch.h.
 */

/*
 * An address value in the i1 stream.  Unlike the i8086 (16-bit ADDRESS), a
 * Z8001 address field carries a full 32-bit displacement: n2/z8001/afield.c
 * reads a symbol-relative offset as two words, high then low, so that byte
 * offsets past 0x7FFF into a large object and negative address folds both
 * survive.  ADDRESS must be that wide or cc3 would print a truncated operand.
 */
typedef unsigned long	ADDRESS;
typedef long		SIGNEDADDRESS;

/* No per-run machine state to set up or tear down. */
#define	cc3init()
#define	cc3close()

/*
 * Opcode-table flags.  Same values as h/z8001/cc2mch.h, because the ins[]
 * table in n3/z8001/icode.c is copied row-for-row from n2/z8001/optab.c.
 * OP_JUMP (01) and OP_DD (02) are MI-owned (h/cc3.h).
 */
#define	OP_BYTE		010	/* byte instruction			*/
#define	OP_NPTR		020	/* data directive (no operand size shown) */
#define	OP_DWORD	040	/* register-pair (long) instruction	*/

/* Assembly comment string: as-z8001 takes '/' or ';' (src/as/asmlex.c). */
#define	CMTSTR	"/"

/* end of h/z8001/cc3mch.h */
