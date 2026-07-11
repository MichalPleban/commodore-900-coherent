/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
/*
 * Coherent.
 * Process trace.
 */
#ifndef	 PTRACE_H
#define	 PTRACE_H
#include <sys/types.h>

/*
 * Structure used for communication between parent and child.
 */
struct ptrace {
	int	 pt_req;		/* Request */
	int	 pt_pid;		/* Process id */
	vaddr_t	 pt_addr;		/* Address */
	int	 pt_data;		/* Data */
	int	 pt_errs;		/* Error status */
	int	 pt_rval;		/* Return value */
	int	 pt_busy;		/* In use */
	GATE	 pt_gate;		/* Gate */
};

#ifdef KERNEL
/*
 * Global variables.
 */
extern	struct	ptrace pts;			/* Ptrace structure */

#endif

#endif
