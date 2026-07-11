/*
 * Machine dependent signals.
 */
#define	SIGEPA	12			/* Extended processor trap (uni) */
#define	SIGPRV	13			/* Privileged instruction */
#define	SIGNVI	14			/* Non vectored interrupt */
#define	SIGNMI	15			/* Non-maskable interrupt (not passed) */
#define	SIGI16	16			/* Signal 16 */
#define	NSIG	16			/* Number of signals */

/*
 * Special arguments to signal.
 */
#define	SIG_DFL	((int(*)())0)		/* Default */
#define	SIG_IGN	((int(*)())1)		/* Ignore */
