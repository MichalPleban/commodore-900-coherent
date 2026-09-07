/*
 * common/z8001/mdlnam.c
 * Machine dependent leaf names.
 * None for the Z8001: the back end adds no leaf operators to ops.h's
 * MDLBASE..MIOBASE range, so every slot is empty and the table exists only
 * because the machine-independent printers (n1/snap0.c, n3/itree.c) index it
 * unconditionally.
 */

#ifdef	vax
#include	"INC$LIB:ops.h"
#else
#include	"ops.h"
#endif

char	*mdlnames[MIOBASE-MDLBASE] = { 0 };

/* end of common/z8001/mdlnam.c */
