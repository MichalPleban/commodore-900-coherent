

/*
 * DFA state
 *	The egrep DFA is composed of these structs.  Operation of the DFA
 * requires the use of d_success and d_p, only.
 */
struct dragon {
	bool		d_success;	/* this is an accepting state */
	struct newt	**d_s;		/* set of NFA states (newts) */
	char		*d_b;		/* bitmap of d_s */
	int		d_hash;		/* hash of d_s */
	struct dragon	*d_next,	/* next dragon (or 0) */
			*d_last,	/* previous dragon (or 0) */
			**d_p;		/* transition vector */
};
