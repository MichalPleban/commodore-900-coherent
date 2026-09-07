/*
 * common/z8001/mdonam.c
 * Machine dependent operator names.
 * None for the Z8001; see common/z8001/mdlnam.c.
 */

#ifdef	vax
#include	"INC$LIB:ops.h"
#else
#include	"ops.h"
#endif

char	*mdonames[ETCBASE-MDOBASE] = { 0 };

/* end of common/z8001/mdonam.c */
