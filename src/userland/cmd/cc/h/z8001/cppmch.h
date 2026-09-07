/*
 * h/z8001/cppmch.h -- preprocessor machine definitions, Segmented Z8001 (== i386 base).
 * i386.
 */

/*
 * n0/cc0.c defines MACHINE, SYSTEM, LOCATION and FPFORMAT for cpp by default.
 * It also defines ISO-compatible versions, e.g. "__COHERENT__".
 * The code in n0/cc0.c knows that MACHINE and FPFORMAT defined below
 * have leading '_' but SYSTEM and LOCATION do not,
 * it must change if the definitions here change.
 */

#define	MACHINE	 "_Z8001"

/*
 * OLDMACHINE is this same target under the spelling the machine's own system
 * sources select on: unprefixed `Z8001'.  That is what the June 1985 compiler's
 * standalone cpp predefines, and what the COHERENT sources written for this
 * machine were written against -- <l.out.h> takes the n.out object layout under
 * `#ifdef Z8001', sbrk() its segment-crossing arithmetic, exec() its
 * shared-library arms.  It is the spelling this system's headers and sources
 * use, so a compiler that defines only the ISO-reserved forms silently builds
 * the wrong layout.
 *
 * MACHINE above keeps the leading underscore because n0/cc0.c derives the
 * doubly-underscored form from it by concatenation; all three spellings are
 * predefined, and new code should say `Z8001'.
 */
#define	OLDMACHINE	"Z8001"

#if	IEEE
#define	FPFORMAT	"_IEEE"
#endif
#if	DECVAX
#define	FPFORMAT	"_DECVAX"
#endif

#ifdef	UDI
#define	LOCATION	"SERIESIII"
#define	SYSTEM		"UDI"
#define	DEFDISK		""
#endif

#ifdef	COHERENT
#define	LOCATION	"MWC"
#define	SYSTEM		"COHERENT"
#ifdef	FLOPPY
#define	DEFDISK		"/lib/include"
#else
#define	DEFDISK		"/usr/include"
#endif
#endif

#ifdef	vax
#define	LOCATION	"VAX"
#define	SYSTEM		"UDI"
#define	DEFDISK		"CC86$INCLUDE:"
#endif

#ifdef	MSDOS
#define	LOCATION	"MWC86"
#define	SYSTEM		"MSDOS"
#define	DEFDISK		""
#endif

/* end of h/i386/cppmch.h */
