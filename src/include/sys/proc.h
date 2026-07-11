/*
 * Process information.
 */
#ifndef  PROC_H
#define  PROC_H
#include <types.h>

/*
 * Number of user segments.
 */
#define NUSEG	8

/*
 * In core information about a process.
 * The `p_lforw' and `p_lback' entries must not move as they must be
 * in the same place as in the `plink' structure.
 */
typedef struct proc {
	struct	 proc *p_lforw;		/* Working forward pointer */
	struct	 proc *p_lback;		/* Working backward pointer */
	struct	 proc *p_nforw;		/* Next forward pointer */
	struct	 proc *p_nback;		/* Next backward pointer */
	struct	 seg  *p_segp[NUSEG+1];	/* Segments */
	unsigned p_pid;			/* Process id */
	unsigned p_ppid;		/* Process id of parent */
	unsigned p_uid;			/* Effective uid */
	unsigned p_ruid;		/* Real uid */
	unsigned p_rgid;		/* Real gid */
	unsigned p_state;		/* Scheduling state */
	unsigned p_flags;		/* Flags */
	sig_t	 p_ssig;		/* Signals which have been set */
	sig_t	 p_isig;		/* Signals which are being ignored */
	char	 *p_event;		/* Wakeup event channel */
	unsigned p_alarm;		/* Timer for alarms */
	unsigned p_group;		/* Process group */
	dev_t	 p_ttdev;		/* Controlling terminal */
	unsigned p_nice;		/* Nice value */
	unsigned p_cval;		/* Cpu schedule value */
	unsigned p_sval;		/* Swap schedule value */
	int	 p_ival;		/* Importance value */
	unsigned p_rval;		/* Response value */
	unsigned p_lctim;		/* Last time cval was updated */
	long	 p_utime;		/* User time (HZ) */
	long	 p_stime;		/* System time */
	long	 p_cutime;		/* Sum of childs user time */
	long	 p_cstime;		/* Sum of childs system time */
	int	 p_exit;		/* Exit status */
} PROC;

/*
 * Segment indices.
 */
#define SIUSERP	0			/* User area segment */
#define SISTACK	1			/* Stack segment */
#define	SISSLIB	2			/* Shared part of shared library */
#define	SIPSLIB	3			/* Private part of shared library */
#define	SISTEXT	4			/* Shared text segment */
#define SIPTEXT	5			/* Private text segment */
#define SISDATA	6			/* Shared data segment */
#define SIPDATA	7			/* Private data segment */
#define SIAUXIL	8			/* Auxiliary segment */

/*
 * Status of process (p_state).
 */
#define PSSLEEP	1			/* Sleeping */
#define PSRUN	2			/* Running */
#define PSDEAD	3			/* Process is exiting */

/*
 * Flags (p_flags).
 */
#define PFCORE	00001			/* Process is in core */
#define PFLOCK	00002			/* Process is locked in core */
#define PFSWIO	00004			/* Swap I/O in progress */
#define PFSWAP	00010			/* Process is swapped out */
#define PFWAIT	00020			/* Process is stopped (not waited) */
#define PFSTOP	00040			/* Process is stopped (waited on) */
#define PFTRAC	00100			/* Process is being traced */
#define PFKERN	00200			/* Kernel process */
#define PFAUXM	00400			/* Auxiliary segments in memory */
#define PFDISP	01000			/* Dispatch at earliest convenience */
#define PFNDMP	02000			/* Command mode forbids dump */
#define	PFSPROTO 04000			/* Must recalculate segment. proto */
#define	PFSLIB	010000			/* Shared library */

/*
 * Values of nice.
 */
#define MINNICE	0			/* Minimum value of nice */
#define DEFNICE	20			/* Default nice value */
#define MAXNICE	40			/* Maximum nice value */

#ifdef KERNEL
/*
 * For easy referencing.
 */
#define p_u	p_segp[SIUSERP]
#define SELF	cprocp

#endif

#ifdef KERNEL
/*
 * Determine if a gate is locked.
 */
#define locked(g)	((g)[0])

#endif

#ifdef KERNEL
/*
 * Functions.
 */
extern	int	idle();
extern	PROC	*process();
extern	int	wakeup();

#endif

#ifdef KERNEL
/*
 * Global variables.
 */
extern	int	quantum;		/* Current quantum */
extern	int	disflag;		/* Dispatch soon as possible */
extern	int	intflag;		/* We are in an interrupt */
#ifndef NOMONITOR
extern	int	swmflag;		/* Monitor swapper */
#endif
extern	int	cpid;			/* Current process id */
extern	GATE	pnxgate;		/* Gate for p_next */
extern	PROC	procq;			/* Process queue */
extern	PROC	*iprocp;		/* Idle process struct */
extern	PROC	*eprocp;		/* Init process struct */
extern	PROC	*cprocp;		/* Current process */
extern	PROC	*slprocp;		/* Shared library */

#endif

/*
 * Number of entries in sleep/wakeup queue.
 */
#define	NHPLINK	32

/*
 * Sleep/wakeup queues.
 */
typedef struct plink {
	struct	 proc *p_lforw;		/* Working forward pointer */
	struct	 proc *p_lback;		/* Working backward pointer */
} PLINK;

#ifdef KERNEL
/*
 * Function to hash a wakeup channel.
 * Most channels are even.
 */
#define hash(e)	((((unsigned)(e))>>1)%NHPLINK)

#endif

#ifdef KERNEL
/*
 * Global variables.
 */
extern	PLINK	linkq[NHPLINK];		/* Sleep/wakeup hash queue */

#endif

#endif
