/*
 * Magic machine numbers.
 * Zilog z8001 (commodore machine)
 */

/* Bits per type */
#define	NBCHAR	  8
#define	NBINT	  16
#define	NBLONG	  32
#define	NBFLOAT	  32
#define	NBDOUBLE  64
#define NBSHORT	  16

/* Bits per pointer */
#ifdef	Z8001
#define	NBPCHAR		32
#define	NBPINT		32
#define	NBPLONG		32
#define	NBPFLOAT	32
#define	NBPDOUBLE	32
#define	NBPSHORT	32
#define	NBPUNION	32
#else
#define	NBPCHAR		16
#define	NBPINT		16
#define NBPLONG		16
#define NBPFLOAT	16
#define	NBPDOUBLE	16
#define NBPSHORT	16
#define	NBPSTRUCT	16
#define NBPUNION	16
#endif

/* Alignments, types */
#define	ALCHAR	  01
#define ALINT	  02
#define ALLONG	  02
#define ALFLOAT	  02
#define ALDOUBLE  02
#define ALSHORT	  02
#define ALSTRUCT  02
#define ALUNION   02

/* Alignments, pointers */
#define	ALPCHAR	  02
#define	ALPINT	  02
#define ALPLONG   02
#define ALPFLOAT  02
#define ALPDOUBLE 02
#define ALPSHORT  02
#define ALPSTRUCT 02
#define ALPUNION  02

/* Ranges */
#define	MAXCHAR	127
#define	MAXUCHAR 255
#define	MAXINT	32767
#define	MAXUINT	65535L
#define	MAXLONG	2147483647L
#define	MAXULONG 4294967295L
#define	MININT	(-32768)
#define MINLONG (-2147483648L)

/* Fixed point representation */
#define	TCINT	1
#define	OCINT	0
#define	SMINT	0

/* Base2 logarithms of bits per type */
#define	LOGCHAR	3
#define	LOGINT	4
#define	LOGLONG	5
