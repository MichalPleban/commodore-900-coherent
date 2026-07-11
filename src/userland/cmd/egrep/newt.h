

/*
 * NFA state
 *	The rex is converted into an NFA composed of these structs.
 */
struct newt {
	char		n_c,		/* label for transition n_cp */
			n_flags;	/* [see below] */
	int		n_uniq,		/* unique # */
			n_id;		/* ID # of this newt */
	char		*n_b;		/* alternative label (char class) */
	struct newt	*n_cp,		/* transition labeled n_c */
			*n_ep,		/* transition labeled EPSILON */
			*n_fp;		/* final newt in this sub-goal */
};

/* n_c
 */
#define	EPSILON	(-1)			/* n_cp is an epsilon transition */

/* n_flags
 */
#define	N_BOL	01		/* beginning-of-line */
#define	N_EOL	02		/* end-of-line */


extern int	uniq;		/* unique # (used to set n_uniq) */
extern int	n_id;		/* # newts (used to set n_id) */
