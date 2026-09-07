/*
 * n1/z8001/mtree2.c
 * Machine-dependent tree rewriting (modoper/modcall/modswap/constcvt/isokareg
 * + field/arg helpers) run after the MI transforms. Segmented Z8001.
 * Template: n1/i8086/mtree2.c (mostly machine-independent). doprolog/doautos
 * here are under #if SPLIT_CC1 (gen1.c owns them in single-phase cc1).
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

int	blkflab;

int	isfarbase();	/* amd.c: base of an address node is a FAR (segmented) pointer */
TREE	*dupnode();	/* deep tree copy (defined below, before modlfld) */
TREE	*mdtempnode();	/* altemp.c: modify-phase stack temporary */

static	char	modoptab[] = {
	'i',	'u',	'i',	'u',	'l',	'v',	'f',
	'd',	'b',	'i',	'i',	'p',	'p',	'p',	'p'
};


#if SPLIT_CC1
/*
 * Function prolog.
 * Clear "BLK function" label.
 */
doprolog()
{
	blkflab = 0;
}

/*
 * Copy auto information.
 */
doautos()
{
	iput(iget());
	iput(iget());
}
#endif

/*
 * A static (named global or static-local) datum whose address, consumed as a VALUE,
 * must be pooled into a real far pointer (poolifbase).  Data/bss bases are the ones the
 * deferred X-mode addressing leaves unpooled; CODE bases (a function used as a
 * value, `gp = fn') are never X-mode-indexed but must also pool -- else the far pointer
 * is two immediate word stores whose SEGMENT word carries no relocation, so a segmented
 * link leaves it seg 0 and an indirect call misfires.
 */
static
isstatbase(g)
register TREE *g;
{
	return (g->t_op==GID || g->t_op==LID)
	    && (g->t_seg==SANY || g->t_seg==SDATA || g->t_seg==SBSS
	     || g->t_seg==SCODE || g->t_seg==SLINK);
}

/*
 * The leaf step (below) leaves a dereferenced static array base UNPOOLED
 * so `g[i]'/`font[k][i]' can address it X-mode (`base(Rindex)') rather than building
 * a far pointer.  When that same static address is instead consumed as a VALUE
 * (`g+i', `&g', a callee argument) it must become a real far pointer.  poolifbase
 * walks an address operand's spine and pools the first such still-unpooled base,
 * rewriting its GID/LID to STAR(poollabel) so the wrapping ADDR(STAR ..) folds to the
 * pooled far pointer on the next pass.  Returns nonzero if it pooled a base.
 */
static
poolifbase(tp)
register TREE *tp;
{
	register TREE *g, *ns;

	if (tp == NULL)
		return 0;
	while (tp->t_op == LEAF)
		tp = tp->t_lp;
	if (tp->t_op == ADD || tp->t_op == SUB)	/* base may sit on either side (2D rotation) */
		return poolifbase(tp->t_lp) | poolifbase(tp->t_rp);
	if (tp->t_op != ADDR)
		return 0;
	g = tp->t_lp;
	while (g != NULL && g->t_op == LEAF)
		g = g->t_lp;
	if (g == NULL || !isstatbase(g))
		return 0;
	pool(g);
	ns = leftnode(STAR, g, g->t_type, g->t_size);
	g->t_type = LPTR;
	g->t_size = 0;
	tp->t_lp = ns;
	return 1;
}

/*
 * True if `bp' is a far-pointer deref whose own address carries a RUNTIME
 * (non-constant) index -- e.g. `m[i]' / `rows[i]', a far pointer LOADED FROM MEMORY at
 * a variable offset.  Such a deref, used as the base of an OUTER indexed deref, must be
 * hoisted to a temp (see modoper).  A plain base+constant slot (`*(FP-n)', the hoisted
 * temp itself, or a fixed field) returns 0 -- so the hoist does not recurse.
 */
static
varidxfar(bp)
register TREE	*bp;
{
	register TREE	*a;

	if (bp->t_op != STAR || !(ispoint(bp->t_type) && islong(bp->t_type)))
		return (0);
	a = bp->t_lp;
	for (;;) {
		while (a->t_op == LEAF)
			a = a->t_lp;
		if (a->t_op == ADD) {
			/* the index is the non-pointer operand (base on either side) */
			if (!ispoint(a->t_rp->t_type)) {
				if (a->t_rp->t_op != ICON && a->t_rp->t_op != LCON)
					return (1);
				a = a->t_lp;
			} else
				a = a->t_rp;
		} else if (a->t_op == SUB) {
			if (a->t_rp->t_op != ICON && a->t_rp->t_op != LCON)
				return (1);
			a = a->t_lp;
		} else
			return (0);
	}
}

/*
 * True if `tp' is a FAR pointer VALUE read from memory -- a deref whose
 * own type is a long (segmented) pointer.  Such a base has to be loaded into a
 * register pair before an index can be added to it, so it can never be part of the
 * addressing mode; an index term grouped against it must be re-associated out.
 */
static
isfarderef(tp)
register TREE *tp;
{
	while (tp->t_op == LEAF)
		tp = tp->t_lp;
	return tp->t_op == STAR && ispoint(tp->t_type) && islong(tp->t_type);
}

/*
 * True if `tp' is a SCALED index term (i*elemsize) -- the one address
 * component gencoll cannot encode, since the Z8000 has no scaled addressing.  Only
 * such a term forces the re-association; a plain variable or constant addend is
 * cheaper left where cc0 put it (it folds straight into the pair's offset half).
 */
static
isscaled(tp)
register TREE *tp;
{
	while (tp->t_op == LEAF)
		tp = tp->t_lp;
	return !ispoint(tp->t_type) && (tp->t_op == SHL || tp->t_op == MUL);
}

/*
 * True if a far pointer belongs on the LEFT of a sum, where add.t's pointer-LEFT
 * rule adds the index into the OFFSET half in one instruction and carries the
 * segment half across untouched.  Two pointer forms belong on the right instead:
 *   - an ADDRESS CONSTANT (a static array name, `&x'), whose int-LEFT rule folds
 *     the base into a displacement and indexes it in X-mode;
 *   - the frame and stack registers, which are NEAR bases with their own int-LEFT
 *     rule (`int + FP', segment half from R14).
 * Everything else -- a frame local, a parameter, a register variable, a loaded
 * pointer, a pool-mediated global, a call, an assignment -- goes left, where the
 * TREG-LPTX subgoal materializes it into a pair if it is not one already.
 */
static
isptrleft(tp)
register TREE *tp;
{
	if ((tp->t_flag&T_CON) != 0)
		return 0;
	if (tp->t_op == REG && (tp->t_reg == FPREG || tp->t_reg == SPREG))
		return 0;
	return 1;
}

/*
 * True if `tp' is a segmented (far) pointer.
 */
static
isfarptr(tp)
register TREE *tp;
{
	return ispoint(tp->t_type) && islong(tp->t_type);
}

/*
 * This function performs machine specific tree modifications.
 * It is called from "modtree" after all of the machine
 * independent transformations have been done.
 * It returns either a pointer to the new tree
 * (telling "modtree" to do another pass)
 * or NULL (which implies that no changes were made).
 */

TREE *
modoper(tp, ac, ptp)
register TREE	*tp;
TREE		*ptp;
{
	register TREE	*lp;
	register TREE	*rp;
	register TREE	*tp1;
	register TREE	*tp2;
	register TREE	*tp3;
	register int	c;
	register int	o;
	register int	op;
	register int	tt;
	register int	nbase;
	register int	ntype;
	register int	lt;
	register int	rt;
	register int	seg;
	register int	lab;
	register int	makecall;

	c = ac;
	if (c==MRETURN || c==MSWITCH || c==MINIT)
		c = MRVALUE;

	/*
	 * Break up cc0's DAG, once per statement.  A NULL parent is the
	 * statement root (mtree0.c's top-level modtree call).
	 */

	/*
	 * Drop a REDUNDANT narrowing cast (int->char) that feeds a byte store:
	 * `cp[i] = (char)n' is identical to `cp[i] = n' (the byte store already
	 * truncates), and the bare-store form is the one selection lowers
	 * correctly.  The explicit CAST otherwise forces a byte READ of the
	 * value, which has no encoding when `n' is a register variable that
	 * landed in R8..R15 (no byte half).
	 */
	if ((tp->t_op == CAST || tp->t_op == CONVERT)
	 && tp->t_lp->t_op == REG	/* a REGISTER-resident value -- the reg
					 * variable that can land in R8..R15; a
					 * CALL/expr result (byte-safe reg R1) is
					 * NOT a REG node here, so it keeps its cast
					 * and its bare store stays selectable */
	 && isbyte(tp->t_type) && !isbyte(tp->t_lp->t_type)
	 && !isflt(tp->t_lp->t_type)
	 && ptp != NULL && ptp->t_op == ASSIGN && ptp->t_rp == tp
	 && ptp->t_lp != NULL && isbyte(ptp->t_lp->t_type)) {
		lp = tp->t_lp;			/* keep the wider value; the byte store
						 * narrows it (identical to `cp[i]=n') */
		return (modtree(lp, ac, ptp));
	}

	/*
	 * Split a LONG->byte narrowing cast into long->int then int->byte:
	 * selection has a rule for each step (long->int takes the low word,
	 * int->byte the low byte) but none for the direct long->byte convert
	 * (cc1 "no match").  `(unsigned char)long_call' in rogue's score.c
	 * (xxx() returns long).
	 */
	if ((tp->t_op == CAST || tp->t_op == CONVERT)
	 && isbyte(tp->t_type) && islong(tp->t_lp->t_type)) {
		tp->t_lp = leftnode(CONVERT, tp->t_lp, S16, 2);
		return (modtree(tp, ac, ptp));
	}
#if !ONLYSMALL&&!NDP
	/*
	 * All fetches of float or double values are rewritten as
	 * routine calls in LARGE model.
	 * This makes the code a great deal smaller,
	 * and solves the problem of addressability on the _fpac_,
	 * which is not in the same segment as the data of the file.
	 */
	if (0	/* Z8001 soft-float has NO ddload/dfload load-helper (i8086 FPAC model);
		 * float<->double conversions are inline ZCALLs to the murphy dfpack/
		 * fdpack (register ABI) from the CONVERT selection rules, not leaf
		 * rewrites -- see leaves.t. */
	&& isvariant(VLARGE)
	&& (ac!=MINIT && ac!=MLADDR && ac!=MRADDR)
	&& (tp->t_flag&T_LEAF) != 0
	&& isflt(tp->t_type)) {
		tp1 = makenode(GID, LPTR);
		if (tp->t_type == F32)
			tp1->t_sp = gidpool("dfload");
		else
			tp1->t_sp = gidpool("ddload");
		tp1->t_seg = SANY;
		tp1 = leftnode(CALL, tp1, F64);
		storedcon(tp);
		if (tp->t_op == STAR)
			tp1->t_rp = tp->t_lp;
		else
			tp1->t_rp = leftnode(ADDR, tp, LPTR);
		return (tp1);
	}
#endif
	op = tp->t_op;
	/*
	 * MI mtree0 folds `x - 0' by RETYPING x in place (a subword access).  For a
	 * FAR POINTER x under a 16-bit-int-typed SUB (`p - (T *)0', the kernel
	 * ptrdiff-vs-NULL idiom) that in-place retype is little-endian-only: the
	 * word at +0 of a stored pair is the SEGMENT on the big-endian Z8001 (and
	 * the high word of RRn in a register).  modoper sees the pointer child
	 * (ptp = the SUB) BEFORE the fold runs -- wrap it in an explicit CONVERT,
	 * so the fold retypes the CONVERT node and selection lowers it through the
	 * endian-correct pointer->int conversion rules instead.
	 */
	if (ptp != NULL && ptp->t_op == SUB
	&& ispoint(tp->t_type) && islong(tp->t_type)
	&& !ispoint(ptp->t_type)
	&& (ptp->t_type == S16 || ptp->t_type == U16)) {
		/* the left child may still be LEAF-wrapped in the parent... */
		for (tp1 = ptp->t_lp; tp1 != NULL && tp1->t_op == LEAF; )
			tp1 = tp1->t_lp;
		/* ...and the right 0 may still be wrapped in unfolded casts */
		for (tp2 = ptp->t_rp; tp2 != NULL
		    && (tp2->t_op==CAST || tp2->t_op==CONVERT || tp2->t_op==LEAF); )
			tp2 = tp2->t_lp;
		if (tp1 == tp && tp2 != NULL && isnval(tp2, 0))
			return (leftnode(CONVERT, tp, ptp->t_type));
	}
	/*
	 * `X = *p++' / `X = *p--' (p a far pointer) as a STATEMENT.  The
	 * postfix rule copies the old pointer into a temp pair so the parent STAR can
	 * deref it; instead, deref straight INTO the assignment target and bump p
	 * afterwards:  (X = *p, p++).  No temp -- the value lands directly in X, so the
	 * pair copy is gone with nothing spilled.  Only an EFFECT-context assignment
	 * (the value is discarded), where the trailing p++ can be the COMMA's discarded
	 * value; a value/flow-context assign keeps the baseline (its value would have to
	 * be re-read past the bump).  The rhs must be a bare STAR(postfix) -- a same-type
	 * store; a CONVERT/widen wrapper or a non-pointer postfix keeps the baseline.
	 * modtree re-walks the COMMA, lowering `X = *p' and the now-effect p++.
	 */
	if (op == ASSIGN && ac == MEFFECT
	&& tp->t_rp != NULL && tp->t_rp->t_op == STAR
	&& tp->t_rp->t_lp != NULL
	&& (tp->t_rp->t_lp->t_op == INCAFT || tp->t_rp->t_lp->t_op == DECAFT)
	&& ispoint(tp->t_rp->t_lp->t_type)) {
		register TREE	*star, *inc, *res;

		star = tp->t_rp;			/* STAR(INCAFT(p,k)) */
		inc = star->t_lp;			/* INCAFT(p,k) */
		star->t_lp = dupnode(inc->t_lp);	/* `X = *p' reads a copy of old p */
		res = leftnode(COMMA, tp, tp->t_type, tp->t_size);
		res->t_rp = inc;			/* (X = *p, p++) */
		return (res);
	}
	/*
	 * Mark a far-pointer field deref `*(p + k)' (k constant) that is a
	 * LOAD/STORE -- i.e. NOT a direct operand of an arithmetic / compare / inc-dec /
	 * bit-field op -- so findoffs (amd.c) FOLDS k into the @RRn+disp addressing (LD/LDL
	 * @RRn+disp exist), saving the materialize ADD.  An unmarked far field stays
	 * materialized (@RRn disp 0), because those ops have no @RRn+disp memory form.
	 */
	if (op == STAR
	&& tp->t_lp->t_op == ADD
	&& (tp->t_lp->t_rp->t_op == ICON || tp->t_lp->t_rp->t_op == LCON)
	&& isfarbase(tp->t_lp)
	&& ac != MFLOW			/* a truth-test is a direct compare (CP @RR) */
	&& !(ptp != NULL
	  && ((ptp->t_op >= ADD && ptp->t_op <= ULT)	/* arith / compare operand */
	   || (ptp->t_op >= INCBEF && ptp->t_op <= DECAFT)	/* inc/dec lvalue */
	   || ptp->t_op == FIELD			/* bit-field */
	   /* store of an IMMEDIATE to this far field: the Z8000 has NO @RRn+disp
	    * immediate-store, so a folded offset would force a seg-0 X-mode workaround
	    * (wrong for a nonzero segment).  Leave it materialized (@RRn disp 0) so the
	    * store uses `LD @RRn,#imm' (segmented via the pair).  A register store, by
	    * contrast, has the @RRn+disp BA form and folds fine. */
	   || (ptp->t_op == ASSIGN && ptp->t_lp == tp
	    && (ptp->t_rp->t_op == ICON || ptp->t_rp->t_op == LCON))
	   || ptp->t_op == CONVERT || ptp->t_op == CAST)))	/* transparent: real
				 * consumer is beyond it (e.g. a long field feeding CPL) */
		tp->t_flag |= T_FOLDOFS;
	/*
	 * A deref of (a FAR-pointer DEREF + a variable index) -- the double
	 * indirection `m[i][j]' with `int **m'.  The address is `m[i] + j*scale': a far
	 * pointer LOADED FROM MEMORY plus a runtime index.  The Z8000 has no @RR+register-
	 * index addressing, so the far base must load into a pair and the index ADDL in --
	 * but cc1's selection spills the inner deref to a frame temp and then leaves the
	 * OUTER index+base unmaterialized, so the address reaches gencoll as
	 * STAR(ADD(index, STAR(...))) and botches ("collect").  Hoist the inner far deref
	 * into a stack temp explicitly, reproducing the hand-split `t = m[i]; t[j]' (which
	 * selects cleanly): *( j*scale + m[i] ) -> ( tmp = m[i], *( j*scale + tmp ) ).
	 * Fires only when the index base is itself a FAR deref (isfarbase, the discriminant
	 * that also bounds the rewrite -- the resulting `*(FP-n)' temp read has a NEAR base)
	 * and the index is a non-constant variable; a far LEAF base (`t[j]') and a constant
	 * index (`m[i][0]', folded earlier) already select.
	 */
	if (op == STAR && tp->t_lp->t_op == ADD && ispoint(tp->t_lp->t_type)) {
		register TREE	*adp, *bp, *xp, *ptmp, *proto, *cm, *outer;

		adp = tp->t_lp;
		bp = NULL;
		xp = NULL;
		if (!ispoint(adp->t_lp->t_type)
		 && adp->t_lp->t_op != ICON && adp->t_lp->t_op != LCON
		 && varidxfar(adp->t_rp)) {
			bp = adp->t_rp; xp = adp->t_lp;		/* cc0 form: index + base */
		} else if (!ispoint(adp->t_rp->t_type)
		 && adp->t_rp->t_op != ICON && adp->t_rp->t_op != LCON
		 && varidxfar(adp->t_lp)) {
			bp = adp->t_lp; xp = adp->t_rp;
		}
		if (bp != NULL) {
			outer = tp;
			proto = makenode(GID, LPTR);
			proto->t_size = pertype[LPTR].p_size;
			ptmp = mdtempnode(proto);		/* *(FP-n): holds the loaded far ptr */
			cm = leftnode(ASSIGN, copynode(ptmp), LPTR, proto->t_size);
			cm->t_rp = bp;				/* tmp = m[i] */
			if (bp == adp->t_rp)
				adp->t_rp = copynode(ptmp);	/* outer base now reads the temp */
			else
				adp->t_lp = copynode(ptmp);
			tp = leftnode(COMMA, cm, outer->t_type, outer->t_size);
			tp->t_rp = outer;			/* (tmp = m[i], *(index + tmp)) */
			return (tp);
		}
	}
	/*
	 * When the hoist above fired on the LVALUE of an assign /
	 * compound-assign / inc-dec (`rows[i][j] = v'), that lhs is now a COMMA
	 * `(tmp = base, *(idx+tmp))'.  sellv cannot address a COMMA lvalue ("no lofs"),
	 * so lift the side effect out above the operator:
	 *   OP(COMMA(a, lval), rhs)  ->  COMMA(a, OP(lval, rhs))
	 * leaving a plain far-pointer+index lvalue for selection (re-walked via goto again).
	 */
	if ((op==ASSIGN || (op>=AADD && op<=ASHR) || (op>=INCBEF && op<=DECAFT))
	 && tp->t_lp->t_op == COMMA) {
		register TREE	*cma;

		cma = tp->t_lp;
		tp->t_lp = cma->t_rp;		/* operator over the real lvalue */
		cma->t_rp = tp;			/* COMMA(a, OP(lval, rhs)) */
		cma->t_type = tp->t_type;
		cma->t_size = tp->t_size;
		return (cma);
	}
	/*
	 * A float/double standing alone in a flow context (if(f) / while(f) / f?:) is
	 * an implicit `f != 0.0'.  The Z8000 has no float TEST, so rewrite it to that
	 * explicit relational, which lowers to a soft-float compare makecall (like the
	 * other float relops).  Without this the bare float truth-test reaches no
	 * terminating selection rule and selfix<->iselect recurses to a stack overflow.
	 */
	if (isflt(tp->t_type) && ac == MFLOW) {
		tp1 = makenode(DCON, tp->t_type);
		for (o = 0; o < sizeof(dval_t); o++)
			tp1->t_dval[o] = 0;		/* 0.0 is all-zero bytes (IEEE) */
		tp = leftnode(NE, tp, TRUTH);
		tp->t_rp = tp1;
		return (tp);
	}
	if (isleaf(op)) {
		if (op==AID || op==PID) {
			/* auto/param -> *(FP + offset); FP = R13 for both models. The
			 * i8086 split BP (near) vs SSBP (stack-seg far); Z8001 has one FP. */
			ntype = SPTR;
			nbase = FPREG;
#if !ONLYSMALL
			if (isvariant(VLARGE)) {
				ntype = LPTR;
				nbase = FPREG;
			}
#endif
			o = tp->t_offs;
			tp->t_op = STAR;
			tp->t_rp = NULL;
			tp->t_lp  = tp2 = makenode(ADD, ntype);
			tp2->t_lp = tp3 = makenode(REG, ntype);
			tp3->t_reg = nbase;
			tp2->t_rp = ivalnode(o);
			return (tp);
		}
#if !ONLYSMALL
		/* Pool a double constant into memory and deref it: the Z8001 soft-float
		 * RQ0 load rule needs the DCON addressable (was VRAM-gated; the segmented
		 * model always pools so a bare `return 1.5' has an RQ0 load). */
		if ((ac != MINIT) && (op == DCON) && isvariant(VLARGE)) {
			pool(tp);	/* the dcon */
			tp->t_type = LPTR;
			tp->t_size = 0;
			pool(tp);	/* the label */
			tp = leftnode(STAR, tp, F64, 0);
			return (tp);
		}
		/*
		 * Build up indirect links to any LID or GID items
		 * that you cannot access in a direct fashon.
		 */
		if (ac != MINIT
		&& (op==LID || op==GID)
		&& (ptp==NULL || ptp->t_op!=CALL || tp!=ptp->t_lp)
		&& isvariant(VLARGE)
		&& !((ptp==NULL || ptp->t_op!=ADDR)
		   && (tp->t_seg==SANY||tp->t_seg==SDATA||tp->t_seg==SBSS))  /* static datum: DA-direct */
		&& !(ptp!=NULL && ptp->t_op==ADDR
		   && (tp->t_seg==SANY||tp->t_seg==SDATA||tp->t_seg==SBSS))) {  /* static array/ptr base: defer to address node, X-mode if dereffed */
			seg = tp->t_seg;
			if (seg==SANY || seg==SDATA || seg==SBSS
			|| (seg==SPURE && isvariant(VRAM))
			|| (seg==SSTRN && notvariant(VROM))) {
				pool(tp);
				tp1=leftnode(STAR, tp, tp->t_type, tp->t_size);
				tp->t_type = LPTR;
				tp->t_size = 0;
				return (tp1);
			}
		}
#endif
		goto done;
	}
	/*
 	 * Beat on lvalue fields.
	 */
	if ((op==ASSIGN || (op>=AADD && op<=ASHR)
	||  (op>=INCBEF && op<=DECAFT))
	&&   tp->t_lp->t_op==FIELD && tp->t_lp->t_type<FLD8)
		return (modlfld(tp, c));
	/*
 	 * Non leaf.
	 * Gather up subtrees.
	 * Rewrite some things as calls to library routines.
	 */
	tt = tp->t_type;
	lp = tp->t_lp;
	if (lp != NULL)
		lt = lp->t_type;
	rp = NULL;
	if (op != FIELD) {
		rp = tp->t_rp;
		if (rp != NULL)
			rt = rp->t_type;
	}
	/*
	 * Z8000 single-index addressing: the static array base must be the addressing
	 * DISPLACEMENT, with every variable index term combined into ONE index register
	 * -- matching the original backend's `LDB Rd, base(Ridx)' with Ridx = i*scale + j.
	 * cc0 left-associates a[i][j] for a CHAR (scale-1 inner index) array as
	 * (i*scale + base) + j, which leaves the scaled term i*scale folded against the
	 * static base -- a scaled address component gencoll cannot encode (botch
	 * "collect").  Rotate the static base out to the top so it folds as the
	 * displacement and the two index addends group into one materialized index:
	 *   (i*scale + base) + j   ->   (i*scale + j) + base
	 * Arrays with element size >= 2 scale BOTH indices, so cc0 already groups them
	 * and never produces this shape; this rotation fires only on the char case.
	 */
	if (op == ADD && ispoint(tt) && rp != NULL && !ispoint(rt) && lp->t_op == ADD) {
		register TREE *base, *idx;
		base = idx = NULL;
		if ((lp->t_rp->t_op==LID || lp->t_rp->t_op==GID) && ispoint(lp->t_rp->t_type)) {
			base = lp->t_rp; idx = lp->t_lp;
		} else if ((lp->t_lp->t_op==LID || lp->t_lp->t_op==GID) && ispoint(lp->t_lp->t_type)) {
			base = lp->t_lp; idx = lp->t_rp;
		}
		if (base != NULL && !ispoint(idx->t_type)) {
			lp->t_lp = idx;		/* inner becomes the two index terms: */
			lp->t_rp = rp;		/* (i*scale) + j */
			lp->t_type = rt;	/* now an integer index sum, not a pointer */
			lp->t_size = rp->t_size;
			tp->t_rp = base;	/* outer: index_sum + static base */
			return (tp);
		}
	}
	/*
	 * The same single-index rule for a base that is a LOADED far pointer
	 * (`byte (*followers)[64]; followers[x][i]') rather than a static array.  The
	 * base is a far deref, so it materializes into a register PAIR and the index
	 * ADDLs in -- there is exactly one index register, and the Z8000 has no
	 * @RRn+register-index form.  cc0 left-associates the char-element case as
	 * (base + i*scale) + j, which leaves a SCALED term grouped against the far base;
	 * gencoll cannot encode a shift inside an address and botches ("collect").
	 * Re-associate so all the variable index terms sum into one and the far base
	 * stays the addressing base:
	 *   (base + i*scale) + j   ->   base + (i*scale + j)
	 * Element sizes >= 2 scale BOTH indices, so cc0 already groups them and never
	 * produces this shape; the rewrite is a no-op there.
	 * Both operand tests are load-bearing for CODE SIZE: a CONSTANT outer addend
	 * folds straight into the pair's offset half where cc0 put it (grouping it with
	 * the scaled term instead spills the pair to the frame), and an UNSCALED inner
	 * addend does too.
	 */
	if (op == ADD && ispoint(tt) && rp != NULL && !ispoint(rt)
	 && rp->t_op != ICON && rp->t_op != LCON
	 && lp->t_op == ADD && ispoint(lp->t_type)) {
		register TREE *base, *idx;

		base = idx = NULL;
		if (isfarderef(lp->t_lp) && isscaled(lp->t_rp)) {
			base = lp->t_lp; idx = lp->t_rp;
		} else if (isfarderef(lp->t_rp) && isscaled(lp->t_lp)) {
			base = lp->t_rp; idx = lp->t_lp;
		}
		if (base != NULL) {
			lp->t_lp = idx;		/* inner becomes the two index terms: */
			lp->t_rp = rp;		/* (i*scale) + j */
			lp->t_type = rt;	/* now an integer index sum, not a pointer */
			lp->t_size = rp->t_size;
			tp->t_lp = base;	/* outer: far base + index_sum */
			tp->t_rp = lp;
			return (tp);
		}
	}
	/*
	 * Far pointer difference (p - q): both operands are 2-word seg:offset pairs
	 * in the same segment (C requires the two pointers address one object), so the
	 * difference is just the WORD subtraction of their OFFSET halves.  Convert each
	 * in the same segment (C requires the two pointers address one object), so the
	 * difference is just the WORD subtraction of their OFFSET halves.  Convert each
	 * operand to the signed word result -- (int)p extracts the offset word -- so the
	 * existing WORD subtraction handles it.  Any element-size divide the front-end
	 * inserted for a non-char pointer wraps this SUB and is unaffected.
	 */
	if (op == SUB && lt == LPTR && rt == LPTR) {
		tp->t_lp = leftnode(CONVERT, lp, S16, 2);
		tp->t_rp = leftnode(CONVERT, rp, S16, 2);
		tp->t_type = S16;
		tp->t_size = 2;
		return (tp);
	}
	/*
	 * A deferred static array/pointer base (left UNPOOLED by the leaf so a
	 * dereferenced index can address it X-mode, `base(Rindex)') must become a real far
	 * pointer once its address is consumed as a VALUE.  The immediate parent decides:
	 * under STAR the address is DEREFERENCED -- leave it (the deref carries the X-mode
	 * displacement); under ADD/SUB it is a sub-address -- defer to the enclosing address
	 * node; otherwise it is a value (g+i, &g, a callee arg, an assignment source) -- pool
	 * the base.  ADD/SUB here must be pointer-typed (an address); a plain integer add is
	 * never an address spine.  NOT in a static initializer (MINIT): there the address is a
	 * link-time constant that iexpr emits inline as a ZGPTR relocation -- pooling would bake
	 * a pointer-to-the-pool-literal (a double pointer) into the datum.
	 */
	if ((((op==ADD || op==SUB) && ispoint(tt)) || op==ADDR) && ac != MINIT) {
		if (ptp==NULL
		|| (ptp->t_op!=STAR && ptp->t_op!=ADD && ptp->t_op!=SUB)) {
			if (poolifbase(tp))
				return (tp);
		}
	}
	/*
	 * char-from-memory == / != a byte-fitting constant.  C widened the byte to int
	 * for the compare (LDB + EXTSB + word CP); for EQUALITY a byte compare against
	 * the same constant is exact when the constant round-trips through the char's
	 * range (signed -128..127, unsigned 0..255).  Drop the widening CONVERT and
	 * narrow the constant so the byte relop emits CPB -- matching the original MWC
	 * backend, which byte-compares in place.  Ordering (< >) keeps the widen.
	 */
	if ((op==EQ || op==NE) && rp!=NULL && rp->t_op==ICON && rp->t_ival != 0
	&&  iswiden(lp) && isbyte(o = lp->t_lp->t_type)) {
		if (o==U8 ? (rp->t_ival >= 0 && rp->t_ival <= 255)
			  : (rp->t_ival >= -128 && rp->t_ival <= 127)) {
			rp->t_type = o;
			rp->t_size = 1;
			tp->t_lp = lp->t_lp;
			return (tp);
		}
	}
	/*
	 * Z8000 has no dedicated right-shift instruction: SRL/SRA are the SAME
	 * SLL/SLA opcode (0xB3) with a NEGATIVE count word -- the decoder infers
	 * the right direction from the count's sign (verified in the sim
	 * decoder).  So a right shift's constant count is stored negated here;
	 * the shift table then emits ZSLL/ZSLA and n2 writes the (negative) count
	 * word verbatim, costing the identical single 2-word SHIFT instruction.
	 * Guard on a positive count so the rewrite is idempotent under a repeated
	 * modtree pass.  (A variable right-shift count is NEG'd at run time before
	 * the dynamic shift -- a Z8000 ISA constraint, not avoidable by opcode
	 * choice; the [ZNEG] is emitted by the variable-count rules in shr.t/ashr.t.)
	 */
	if ((op==SHR || op==ASHR) && rp!=NULL && rp->t_op==ICON && rp->t_ival > 0)
		rp->t_ival = -rp->t_ival;
	/*
	 * Unsigned divide/remainder by a CONSTANT with its top bit set: the signed
	 * Z8000 DIV/DIVL reads such a divisor as negative, but the quotient is just
	 * 0 or 1 (the dividend < 2^N < 2*C).  x / C -> (x >= C); x % C -> (x >= C) ?
	 * x-C : x.  Cheap, and emitted only for this constant corner (the common
	 * path is untouched).  REM duplicates the dividend, so skip a side-effecting
	 * one -- the word case then falls into the 32-bit divide below.
	 *
	 * A compound assignment is rewritten to its binary form first (`x /= C' ->
	 * `x = x / C'), so that the quotient below serves it too: there is no wider
	 * divisor for a 32-bit divide to widen into, so the constant would otherwise
	 * reach DIVL and be read as negative.  The lvalue is read a second time, so
	 * this needs one that can be: a leaf names a variable and nothing more.
	 */
	if ((op==ADIV || op==AREM) && tt==U32
	&&  rp!=NULL && rp->t_op==LCON && (upper(rp->t_lval) & 0x8000)
	&&  lp!=NULL && (lp->t_flag&T_LEAF) != 0) {
		register TREE	*bin;

		bin = makenode(op - (AADD - ADD), tt);
		bin->t_size = tp->t_size;
		bin->t_lp = copynode(lp);
		bin->t_rp = rp;
		tp->t_op = ASSIGN;
		tp->t_rp = bin;
		return (tp);
	}
	if ((op==DIV || op==REM)
	&& ((tt==U16 && rp!=NULL && rp->t_op==ICON && (rp->t_ival & 0x8000))
	||  (tt==U32 && rp!=NULL && rp->t_op==LCON && (upper(rp->t_lval) & 0x8000)))) {
		if (op==DIV) {
			/*
			 * The compare's value is an int.  At 16 bits that is the width the
			 * node already has, so the node becomes the compare; a 32-bit
			 * quotient needs the int widened back, and the node becomes that
			 * CONVERT (a relational typed long matches no selection rule).
			 */
			if (islong(tt)) {
				tp1 = leftnode(UGE, lp, TRUTH);
				tp1->t_rp = rp;
				tp->t_op = CONVERT;
				tp->t_lp = tp1;
				tp->t_rp = NULL;
				return (tp);
			}
			tp->t_op = UGE;
			return (tp);
		}
		if (!hascall(lp)) {
			tp1 = leftnode(UGE, copynode(lp), TRUTH);
			tp1->t_rp = copynode(rp);
			tp2 = leftnode(SUB, copynode(lp), tt);
			tp2->t_rp = rp;
			tp3 = leftnode(COLON, tp2, tt);
			tp3->t_rp = lp;
			tp->t_op = QUEST;
			tp->t_lp = tp1;
			tp->t_rp = tp3;
			return (tp);
		}
	}
	/*
	 * Unsigned WORD divide/remainder whose divisor the compiler cannot bound:
	 * the Z8000 has no unsigned divide, and its one-word DIV reads a 16-bit
	 * divisor with bit 15 set as NEGATIVE, so that instruction can only serve a
	 * divisor known to be under 0x8000 -- which only a constant is here.
	 * Widen the DIVISOR to 32 bits (zero-extended, hence positive)
	 * and divide with DIVL, whose divisor is a full word wider than any U16
	 * value: the UWORD rules with a LONG right operand in div/rem/adiv/arem.t
	 * take the 16-bit dividend into the low word of the 64-bit DIVL dividend and
	 * leave a 16-bit result, so the node's own type and the lvalue store are
	 * unchanged.  A divisor that is a constant with bit 15 clear keeps the
	 * one-word DIV above; a constant with bit 15 set was rewritten above.
	 */
	if ((op==DIV || op==REM || op==ADIV || op==AREM) && tt==U16
	&&  rp!=NULL && !islong(rt)
	&& !(rp->t_op==ICON && (rp->t_ival & 0x8000) == 0)) {
		tp->t_rp = leftnode(CONVERT, rp, U32, 4);
		return (tp);
	}
	/*
	 * Long MUL, DIV and REM are always a function call.
	 * If there is no NDP in the machine,
	 * then floating point is a routine call.
	 * If there is, we rewrite conversions from bytes and unsigned things
	 * and all conversions from float to fixed
	 * (a mode switch may be necessary).
	 */
	/*
	 * Float compound-assign (`d *= x', `d += x'): there is no F64 aadd/amul
	 * selection rule, and only the BINARY float ops makecall below -- so a
	 * float `lhs op= rhs' reaches selection unlowered and "no match"es
	 * (EDFA69, seen in printf's _dtoa `d *= _powtab[i]').  Expand it to
	 * `lhs = lhs <binop> rhs': the arithmetic compound-assign ops (AADD..AREM)
	 * sit exactly (AADD-ADD) past their binary ops, so the rewritten binary op
	 * lowers to its soft-float call (dmul/dadd/...) and the ASSIGN stores the
	 * F64 result.  Only the arithmetic compound-assigns are ever float
	 * (AAND..ASHR never are).
	 */
	if (isflt(tt) && op >= AADD && op <= AREM) {
		register TREE	*rhs;

		rhs = makenode(op - (AADD - ADD), tt);
		rhs->t_size = tp->t_size;
		rhs->t_lp = copynode(lp);		/* read the lhs */
		rhs->t_rp = rp;
		tp->t_op = ASSIGN;
		tp->t_rp = rhs;
		return (tp);	/* non-NULL -> modtree re-walks: the new MUL lowers to dmul */
	}
	makecall = 0;
	if (0) {	/* long MUL/DIV/REM are native on the Z8001 (MULTL/DIVL) */
		++makecall;
#if NDP
	} else if (isflt(tt)) {
		if (op==CONVERT || op==CAST) {
			if ((lp->t_flag&T_REG) != 0
			|| (lp->t_flag&T_LEAF) == 0
			|| (lt!=S16 && lt!=S32 && lt!=F32))
				++makecall;
		}
	} else if ((op>=GT && op<=LT) && isflt(lt)) {
		tp->t_op += UGT-GT;
#else
	} else if (isflt(tt)) {
		if (op==CONVERT || op==CAST) {
			/* int->float is a makecall (d<src>flt); float<->double is NOT --
			 * those lower to inline ZCALLs of dfpack/fdpack in the CONVERT
			 * selection rules (register ABI), like the i386 _dfcvt. */
			if (!isflt(lt))
				++makecall;
		} else if (op>=ADD && op<=REM)
			++makecall;
	} else if (isrelop(op) && isflt(lt)) {
		++makecall;
#endif
	} else if ((op==CONVERT || op==CAST) && isflt(lt))
		++makecall;
	if (makecall != 0)
		return (modxfun(tp));
	switch (op) {

	case EQ: case NE: case GT: case GE: case LE: case LT:
	case UGT: case UGE: case ULE: case ULT:
		/*
		 * K&R pun COMPARISON `ptr <op> int' (`dp < FP_OFF(..)+BSIZE'):
		 * as with the pun ASSIGN below, cc0 leaves raw mixed operands.
		 * Wrap the 16-bit side in a CONVERT to the pointer type so the
		 * compare selects as a full pair compare (CPL) -- the int side
		 * materializes as offset/seg 0, consistent with the pun ASSIGN.
		 */
		if (ispoint(lt) && islong(lt)
		&& !ispoint(rt) && (rt == S16 || rt == U16)) {
			tp->t_rp = leftnode(CONVERT, rp, lt);
			return (tp);
		}
		if (ispoint(rt) && islong(rt)
		&& !ispoint(lt) && (lt == S16 || lt == U16)) {
			tp->t_lp = leftnode(CONVERT, lp, rt);
			return (tp);
		}
		/* ptr <op> LONG (`dp < FP_OFF(..)+BSIZE' with faddr_t = long):
		 * same machine kind (32-bit pair) -- retype the POINTER side to
		 * the long type so the CPL pair-compare rules match.  (Retyping
		 * the long side instead loops: if it is an ADD, MI fixaddtype
		 * re-derives its type from the children every modtree pass,
		 * undoing the retype.)  The pointer side is a leaf load whose
		 * type nothing recomputes.  No code difference -- an unsigned
		 * 32-bit compare IS the address compare. */
		if (ispoint(lt) && islong(lt)
		&& lp->t_op != ADD && lp->t_op != SUB
		&& !ispoint(rt) && (rt == S32 || rt == U32)) {
			lp->t_type = rt;
			return (tp);
		}
		if (ispoint(rt) && islong(rt)
		&& rp->t_op != ADD && rp->t_op != SUB
		&& !ispoint(lt) && (lt == S32 || lt == U32)) {
			rp->t_type = lt;
			return (tp);
		}
		break;

	case FIELD:
		if (c != MLADDR)
			return (modefld(tp->t_lp, tp, c, 1));
		break;

	case INCBEF:
	case INCAFT:
	case DECBEF:
	case DECAFT:
		/* A 32-bit long has no native INC, and the load-modify-store needs a temp
		 * the inc/dec tables can't supply, so a long ++/-- is rewritten via the
		 * (working) long compound-assign:  ++l == (l += 1);  l++ (for effect) == l += 1.
		 * Pre ++l/--l (value = new) and post l++/l-- used only for effect map straight
		 * onto it.  A post ++/-- whose OLD value is used needs a second temp the rewrite
		 * can't supply (wrapping (l-=1)+1 doesn't propagate the value through the
		 * compound-assign), so leave it a clean no-match rather than miscompile -- needs
		 * a 2-temp aft.t LONG rule.  The amount is a 32-bit LCON, not an int ICON (an
		 * ICON widened to S32 has an undefined high word). */
		if (tt == S32 || tt == U32) {
			if ((op==INCAFT || op==DECAFT) && ac != MEFFECT)
				break;
			tp->t_op = (op==DECBEF || op==DECAFT) ? ASUB : AADD;
			tp->t_rp = lvalnode((lval_t)1);
			tp->t_rp->t_type = tt;
			return (modoper(tp, ac, NULL));
		}
		break;

	case ASSIGN:
		/*
		 * K&R `ptr = int' / `int = ptr' (the Strict "integer pointer pun"):
		 * cc0 leaves the ASSIGN with RAW mixed operands, no CONVERT node --
		 * e.g. the 3.2 kernel's `dp = FP_OFF(...)' small-model idiom.  Insert
		 * the explicit CONVERT so selection lowers it through the (endian-
		 * correct) pointer<->int conversion rules: int->ptr materializes
		 * offset=int/segment=0 (the donor i8086 contract), ptr->int takes the
		 * offset word.  Types must otherwise select nothing (no match).
		 */
		if (ispoint(tt) && islong(tt)
		&& rp != NULL && !ispoint(rp->t_type)
		&& (rp->t_type == S16 || rp->t_type == U16)) {
			tp->t_rp = leftnode(CONVERT, rp, tt);
			return (tp);
		}
		if (!ispoint(tt) && (tt == S16 || tt == U16)
		&& rp != NULL && ispoint(rp->t_type) && islong(rp->t_type)) {
			tp->t_rp = leftnode(CONVERT, rp, tt);
			return (tp);
		}
		if (tt == BLK) {
			tp = modsasg(lp, rp, tp->t_size, c != MEFFECT);
			if (c != MEFFECT)
				tp = leftnode(STAR, tp, BLK, tp->t_size);
			return (tp);
		}
		/* Narrowing store: a double value into a float lvalue.  cc0 computes in
		 * double and leaves the ASSIGN typed F64 over an F32 lvalue with no
		 * CONVERT node (a hardware FPU would narrow on store).  Insert an explicit
		 * F64->F32 CONVERT -- selection lowers it to an inline fdpack ZCALL (the
		 * FLT<-DBL rule in leaves.t) -- and retype the ASSIGN to F32 so the 4-byte
		 * float result is stored by the F32 store rule. */
		if (tt == F64 && lp->t_type == F32) {
			tp->t_rp = leftnode(CONVERT, rp, F32);
			tp->t_type = F32;
		}
		break;

	case AADD:
	case ASUB:
	case AMUL:
	case ADIV:
	case AREM:
		if (isflt(tt)) {
			tp1 = makenode((op - AADD) + ADD, tt);
			tp1->t_lp = dupnode(tp->t_lp);
			tp1->t_rp = tp->t_rp;
			tp->t_op = ASSIGN;
			tp->t_rp = modoper(tp1, MRVALUE, NULL);  /* lower `x op y' to its dradd/... call first */
			return (modoper(tp, ac, NULL));
		}
		break;

	case NEG:
		/* Float/double unary minus.  The Z8001 soft-float has no dneg helper, and
		 * the DECVAX double format has no negative zero, so -x == 0.0 - x EXACTLY.
		 * Rewrite to the (working) drsub/dlsub SUB lowering.  (Integer NEG falls
		 * through to the ZNEG/ZSBC table rules unchanged.) */
		if (isflt(tt)) {
			tp1 = makenode(DCON, tt);
			for (o = 0; o < sizeof(dval_t); o++)
				tp1->t_dval[o] = 0;		/* 0.0 = all-zero bytes */
			tp->t_op = SUB;
			tp->t_rp = tp->t_lp;		/* operand -> subtrahend */
			tp->t_lp = tp1;			/* 0.0 -> minuend */
			return (modoper(tp, ac, NULL));
		}
		break;

	case SHR:
	case ASHR:
	case DIV:
	case REM:
		/* The shift KIND -- logical (SRL) vs arithmetic (SRA) -- follows the LEFT
		 * OPERAND's signedness (C: `E1>>E2' has the promoted type of E1), and the
		 * divide/remainder KIND (EXTS vs CLR-high dividend setup) follows the
		 * OPERANDS' common signedness.  cc0 can fold a (signed)/(unsigned) cast
		 * INTO an expression-context shift or divide (expr.c adjust), leaving the
		 * RESULT labeled with the OPPOSITE signedness from the operand -- e.g.
		 * `(int)(u>>n)' -> a SHR with a signed result but an unsigned operand.  The
		 * shr.t rules dispatch on the RESULT type, so that mixed shift matches NO
		 * rule and ICEs ("no match"); a mixed DIV/REM selects the WRONG divide.
		 * The CONVERT case below already handles the form where the cast SURVIVES
		 * as a node (the return-coerce path, kept by the stat.c guard); this
		 * catches the cc0-FOLDED form that reaches cc1 with no CONVERT.  Re-sync
		 * the result signedness to the operand so the correct rule selects -- the
		 * value is exactly what the operand's form yields (the cast was a pure
		 * reinterpretation of the same bits).  Same machine kind only (a
		 * width-changing convert is a real op, not a relabel). */
		if (lp != NULL && modkind(tt) == modkind(lp->t_type)
		 && isuns(tt) != isuns(lp->t_type))
			tp->t_type = lp->t_type;
		break;

	case CONVERT:
	case CAST:
		if (modkind(tt) == modkind(lp->t_type)) {
			/*
			 * Same machine kind (same regs/bits) so the convert emits
			 * no code -- drop it.  BUT for DIV/REM (EXTS vs CLR dividend
			 * setup) and >> SHR/ASHR (SRA arithmetic vs SRL logical) the
			 * operand's own SIGNEDNESS picks the instruction and thus
			 * changes the RESULT, not just its interpretation.  Two rules:
			 *
			 * 1. A signedness-changing convert over one of those ops,
			 *    CONSUMED BY a sign-sensitive parent (another shift/
			 *    divide, or a widening/float convert whose EXTS-vs-CLR
			 *    setup reads the operand's signedness), is a REAL
			 *    boundary: without the node, `(int)(u/d) >> k' and
			 *    `(int)((u/d) >> k)' are the same tree.  KEEP it --
			 *    selection's same-kind identity rule (leaves.t) shares
			 *    the operand's register and emits nothing.  A sign-blind
			 *    parent (add, compare -- the compare OP already encodes
			 *    signedness) needs no boundary, so the node drops and
			 *    costs nothing.
			 *
			 * 2. On the drop path, do NOT relabel an unsigned one as
			 *    signed (e.g. `return (unsigned)x >> n' or `.../...'
			 *    from an int fn) -- keep its type.  (A relabel the other
			 *    way is repaired by the SHR/DIV re-sync on the next
			 *    modtree pass.)
			 */
			if ((lp->t_op == DIV || lp->t_op == REM
			  || lp->t_op == SHR || lp->t_op == ASHR
			  || lp->t_op == ADIV || lp->t_op == AREM)
			 && isuns(tt) != isuns(lp->t_type)
			 && ptp != NULL
			 && (ptp->t_op == DIV || ptp->t_op == REM
			  || ptp->t_op == SHR || ptp->t_op == ASHR
			  || ptp->t_op == ADIV || ptp->t_op == AREM
			  || ptp->t_op == CONVERT || ptp->t_op == CAST))
				break;
			if (!((lp->t_op == DIV || lp->t_op == REM
			       || lp->t_op == SHR || lp->t_op == ASHR
			       || lp->t_op == ADIV || lp->t_op == AREM)
			      && isuns(lp->t_type)))
				lp->t_type = tt;
			return (lp);
		}
	}
	/*
	 * If this tree is the return value of a structure function,
	 * arrange to copy the value into a secret place
	 * and return the address of the place.
	 */
done:
	if (tp->t_type==BLK && ac==MRETURN) {
		if (blkflab == 0) {
			blkflab = newlab();
			o = newseg(SBSS);
			genlab(blkflab);
			bput(BLOCK);
			zput(tp->t_size);	/* BSS size: zput (sizeof_t width) to match
						 * the BLOCK reader; iput (ival_t) under-writes
						 * it and n2's lget reads a garbage size. */
			newseg(o);
		}
		lp = makenode(LID, BLK, tp->t_size);
		lp->t_label = blkflab;
		lp->t_seg = SBSS;
		return (modsasg(lp, tp, tp->t_size, 1));
	}
	return (NULL);
}

/*
 * Given a type, return a kind that is used to see
 * if two objects are just different names for the same bits.
 */
modkind(t)
register t;
{
	if (t == U16)
		t = S16;
	else if (t == U32 || t == LPTR || t == LPTB)
		t = S32;
	return (t);
}

/*
 * Size (bytes) at/under which an aggregate assignment is copied INLINE, one
 * long word / word / byte at a time, instead of by the Z8000 block move.
 *
 * A block move costs a fixed preamble -- both far addresses into register
 * pairs, the count into a word register, and, since those pairs are callee
 * saved, a PUSHL/POPL of each when the enclosing function does not already
 * hold them -- against which the inline copy pays 12 bytes and 32 clocks per
 * long word (`LDL RRn,addr' 15 + `LDL addr,RRn' 17, segmented long offset).
 * Whole-function figures for a static-to-static copy, inline vs block move
 * (bytes / clocks of the copy):
 *
 *	bytes copied	 2	 4	 6	 8	10	12
 *	inline		12/26	12/32	24/58	24/64	36/90	36/96
 *	block move	26/112	26/130	26/148	26/166	26/184	26/202
 *
 * so through eight bytes the inline copy is at least as short AND faster, and
 * from ten it is faster but longer.  Eight is where it stops winning both.
 */
#define	INLINEBLK	8

/*
 * Build the lvalue for the (ty,sz) scalar at byte offset `off` within aggregate
 * lvalue `lv`:  *((char *)&lv + off).  &lv collapses to lv's address expression
 * (ADDR-of-STAR cancels for autos; LDA for statics), so for an FP-relative auto
 * this selects as a plain X-mode access  off(FP).  copynode is the established
 * lvalue-duplication idiom (cf. mtree0 postfix/ternary); leaves are not mutated.
 */
TREE *
blkword(lv, off, ty, sz, ptdt)
register TREE	*lv;
{
	register TREE	*tp;

	tp = leftnode(ADDR, copynode(lv), ptdt, 0);
	tp = leftnode(ADD, tp, ptdt, 0);
	tp->t_rp = ivalnode((ival_t)off);
	tp = leftnode(STAR, tp, ty, sz);
	return (tp);
}

/*
 * True if the tree contains a CALL -- a side-effecting / non-repeatable
 * subexpression that must NOT be duplicated by the per-word struct copy.
 * (Otherwise `q = mk(x)' with mk() returning a struct would call mk once per
 * copied word, and the duplicated call result leaks a bare REG into output.)
 */
hascall(tp)
register TREE	*tp;
{
	if (tp == NULL)
		return (0);
	if (tp->t_op == CALL)
		return (1);
	if (tp->t_op < ADD)		/* leaf operand: t_lp/t_rp are not children */
		return (0);
	return (hascall(tp->t_lp) || hascall(tp->t_rp));
}

/*
 * True if a tree is NON-REPEATABLE -- a CALL or a side-effecting op (++/--,
 * =, the compound assigns).  blkword() copies its operand ONCE PER WORD, so a
 * non-repeatable ADDRESS (`*++vp = s', `*p++ = s') would run the side effect
 * per word AND leaves a bare register the address selection can't reload.
 * Bind it into a temp first.
 */
static
nonrepeat(tp)
register TREE	*tp;
{
	register int	op;

	if (tp == NULL)
		return (0);
	op = tp->t_op;
	if (op == CALL || op == ASSIGN
	 || (op >= AADD && op <= AREM)
	 || (op >= INCBEF && op <= DECAFT))
		return (1);
	if (op < ADD)			/* leaf: t_lp/t_rp are not children */
		return (0);
	return (nonrepeat(tp->t_lp) || nonrepeat(tp->t_rp));
}

/*
 * Aggregate (struct/array) assignment.  Small blocks are copied inline word by
 * word; larger blocks become a single Z8000 LDIRB (the BLKMOVE node).  The size
 * is valid.  `blkval' asks for the assignment's VALUE: with it the tree yields
 * the destination address (the pointer type), for an rvalue use of the
 * assignment or a struct return; without it the tree is effect-only.
 */
TREE *
modsasg(lp, rp, s, blkval)
register TREE	*lp;
register TREE	*rp;
int		blkval;
{
	register TREE	*tp;
	TREE		*prebind, *predst;
	int		nptdt;

	nptdt = SPTR;
#if !ONLYSMALL
	if (isvariant(VLARGE))
		nptdt = LPTR;
#endif
	if (s <= 0) {
		/*
		 * An aggregate of no bytes.  There is nothing to copy, and a
		 * zero count is the one value the block move cannot express:
		 * LDIRB decrements its counter and then tests it, so r = 0
		 * means 65536 bytes.  Evaluate both addresses -- the side
		 * effects a block move would have run -- and yield &dest.
		 */
		tp = leftnode(COMMA,
			leftnode(ADDR, rp, nptdt, pertype[nptdt].p_size),
			nptdt, pertype[nptdt].p_size);
		tp->t_rp = leftnode(ADDR, lp, nptdt, pertype[nptdt].p_size);
		return (tp);
	}
	if (s <= INLINEBLK
	 && !hascall(rp) && rp->t_op != STAR && !nonrepeat(lp)) {
		/*
		 * Small block, and both operand trees are REPEATABLE: blkword()
		 * copies each operand once per word, so a source that is a call
		 * (`q = mk(x)') or a pointer dereference (`*q', `p->agg'), or a
		 * destination with a side effect (`*p++ = s'), must not come
		 * through here -- the call would run per word, the deref would
		 * re-deref the register already holding the pointer, and the side
		 * effect would repeat.  Those shapes take the block move below,
		 * which evaluates each address exactly once and is the smaller
		 * form for them anyway.
		 */
		register TREE	*chain;
		register int	off;
		int		ty, sz;

		chain = NULL;
		for (off = 0; off < s; off += sz) {
			/*
			 * Widest chunk that fits, because one long-word copy is
			 * cheaper than the two word copies it replaces on every
			 * addressing mode: 15+17 clocks and 12 bytes against
			 * 2*(12+14) and 24 direct, 16+18 against 2*(13+15)
			 * indexed.  A long word needs the same EVEN address a
			 * word does -- the Z8001 splits it into two word
			 * transfers -- and every aggregate whose size reaches
			 * two bytes is word aligned, so the chunk sequence
			 * 4,4,...,{2},{1} never lands odd.
			 */
			if (s - off >= 4) { ty = S32; sz = 4; }
			else if (s - off >= 2) { ty = S16; sz = 2; }
			else		  { ty = S8;  sz = 1; }
			/*
			 * dupnode, NOT the same node each time: every member store
			 * gets its OWN copy of the address subtree.  Sharing it makes
			 * the statement a DAG, and everything downstream -- modfold's
			 * associative rebuild, foldaddr's offset fold, selection's
			 * relabelling -- rewrites nodes IN PLACE, which is only sound
			 * for a node with one parent.  Shared, one member's offset
			 * folds into the shared base and the stores walk off the
			 * object.
			 */
			tp = blkword(dupnode(lp), off, ty, sz, nptdt);
			tp = leftnode(ASSIGN, tp, ty, sz);
			tp->t_rp = blkword(dupnode(rp), off, ty, sz, nptdt);
			if (chain == NULL)
				chain = tp;
			else {
				register TREE	*cm;
				cm = leftnode(COMMA, chain, ty, sz);
				cm->t_rp = tp;
				chain = cm;
			}
		}
		/*
		 * The VALUE of an aggregate assignment is the destination ADDRESS:
		 * the ASSIGN case STARs it for an rvalue use, and a struct-returning
		 * function returns it (in RR0) so the caller can read the result.
		 * Append `&dest' as the comma chain's final (value) operand -- the
		 * word copies run for effect, the address is the result.  Only a
		 * value context gets it, the same contract the block move below
		 * ends on: in MEFFECT the address is dead, and materializing it
		 * costs a load of the far pointer nothing reads.
		 */
		if (blkval) {
			register TREE	*cm;

			tp = leftnode(ADDR, dupnode(lp), nptdt,
				pertype[nptdt].p_size);
			cm = leftnode(COMMA, chain, nptdt, pertype[nptdt].p_size);
			cm->t_rp = tp;
			chain = cm;
		}
		return (chain);
	}
	/*
	 * Everything else: inline Z8000 block move.  The Z8000 LDIRB copies a whole
	 * byte block in ONE (interruptible, self-repeating) instruction given the
	 * far dst/src addresses in register PAIRS and the count in a register --
	 * no runtime memcpy call (the i8086 backend's `blkmv' GID).  Build a
	 * BLKMOVE node whose subtrees are the far-pointer dst (t_lp) and src
	 * (t_rp) addresses; blkmv.t lowers it to `LD Rc,#size; LDIRB @dst,@src,Rc'.
	 */
	/*
	 * A non-repeatable dst/src address (`*++vp = s', or a struct-returning
	 * call source) must be evaluated ONCE: BLKMOVE derefs its address
	 * operands, and a re-evaluated side effect (or duplicated call) is
	 * wrong -- the same hoist the inline path above does.
	 */
	predst = NULL;
	if (nonrepeat(lp)) {
		register TREE	*proto, *ptmp;

		proto = makenode(GID, nptdt);
		proto->t_size = pertype[nptdt].p_size;
		ptmp = mdtempnode(proto);
		predst = leftnode(ASSIGN, copynode(ptmp), nptdt, proto->t_size);
		predst->t_rp = leftnode(ADDR, lp, nptdt, proto->t_size);
		lp = leftnode(STAR, copynode(ptmp), BLK, s);
	}
	prebind = NULL;
	if (hascall(rp)) {
		register TREE	*proto, *ptmp;

		proto = makenode(GID, nptdt);
		proto->t_size = pertype[nptdt].p_size;
		ptmp = mdtempnode(proto);
		prebind = leftnode(ASSIGN, copynode(ptmp), nptdt, proto->t_size);
		prebind->t_rp = leftnode(ADDR, rp, nptdt, proto->t_size);
		rp = leftnode(STAR, copynode(ptmp), BLK, s);
	}
	rp = leftnode(ADDR, rp, nptdt);		/* src far pointer */
	lp = leftnode(ADDR, lp, nptdt);		/* dst far pointer */
	tp = leftnode(BLKMOVE, blkval ? dupnode(lp) : lp, S16);
	tp->t_rp = rp;
	tp->t_size = s;
	/*
	 * The VALUE of an aggregate assignment is the destination ADDRESS, the
	 * same contract the small-block path above ends on: the ASSIGN case
	 * STARs it for an rvalue use (`a = b = c'), and a struct-returning
	 * function returns it so the caller can read the result.  BLKMOVE
	 * itself is an effect-only node (blkmv.t has a PEFFECT rule and no
	 * other), so a value context needs `&dest' comma'd on after it.  Only
	 * a value context gets it: in MEFFECT the address is dead and the
	 * bare BLKMOVE is what the table wants.
	 */
	if (blkval) {
		register TREE	*cm;
		cm = leftnode(COMMA, tp, nptdt, pertype[nptdt].p_size);
		cm->t_rp = lp;
		tp = cm;
	}
	if (prebind != NULL) {			/* src address once, before the move */
		register TREE	*cm;
		cm = leftnode(COMMA, prebind, nptdt, pertype[nptdt].p_size);
		cm->t_rp = tp;
		tp = cm;
	}
	if (predst != NULL) {			/* dst address (its side effect) once */
		register TREE	*cm;
		cm = leftnode(COMMA, predst, nptdt, pertype[nptdt].p_size);
		cm->t_rp = tp;
		tp = cm;
	}
	return (tp);
}

/*
 * Modify function calls.
 * Handle functions that return objects of type "BLK"
 * by adding a free indirection node.
 */
/*
 * True if an address tree is too hard for gencoll to render as ONE
 * operand: anything beyond leaf chains, a lone register/symbol base and
 * constant offsets (a second variable term, a scale, an inner STAR).
 */
static int
hardaddr(tp)
register TREE *tp;
{
	for (;;) {
		if (tp == NULL)
			return (0);
		switch (tp->t_op) {
		case LEAF:
			tp = tp->t_lp;
			continue;
		case GID:
		case LID:
		case REG:
		case ICON:
		case LCON:
			return (0);
		case ADD:
		case SUB:
			if (tp->t_rp != NULL
			 && (tp->t_rp->t_op == ICON || tp->t_rp->t_op == LCON)) {
				tp = tp->t_lp;
				continue;
			}
			if (tp->t_op == ADD && tp->t_lp != NULL
			 && (tp->t_lp->t_op == ICON || tp->t_lp->t_op == LCON)) {
				tp = tp->t_rp;
				continue;
			}
			return (1);
		default:
			return (1);
		}
	}
}

/*
 * The callee of an indirect call is STAR(inner).  gencoll renders ONE
 * memory indirection natively (the operand is the fn pointer's home:
 * inner = STAR(y) with collectible y), so only deeper/indexed shapes
 * need the spill.
 */
static int
calleehard(tp)
register TREE *tp;
{
	while (tp != NULL && tp->t_op == LEAF)
		tp = tp->t_lp;
	if (tp == NULL)
		return (0);
	if (tp->t_op == STAR)
		return (hardaddr(tp->t_lp));
	return (hardaddr(tp));
}

/*
 * An indirect call names a function-pointer OBJECT.  A static one is DA-direct
 * like any other datum, but the CALL rules take a directly addressable
 * callee as the call TARGET, so a DA-direct pointer object would be called at
 * its own address instead of through its contents.  In the callee position the
 * pointer must therefore be a real far pointer: pool its address and deref the
 * pool cell, exactly as modleaf does for the operands it cannot address.
 */
static
poolcallee(tpp)
TREE **tpp;
{
	register TREE	*gp, *sp;
	register int	seg;

	if (!isvariant(VLARGE))
		return (0);
	if ((sp = *tpp) == NULL || sp->t_op != STAR)
		return (0);
	tpp = &sp->t_lp;
	while ((gp = *tpp) != NULL && gp->t_op == LEAF)
		tpp = &gp->t_lp;
	if (gp == NULL || (gp->t_op != GID && gp->t_op != LID))
		return (0);
	if (!ispoint(gp->t_type))
		return (0);
	seg = gp->t_seg;
	if (seg != SANY && seg != SDATA && seg != SBSS
	 && !(seg == SPURE && isvariant(VRAM))
	 && !(seg == SSTRN && notvariant(VROM)))
		return (0);
	pool(gp);
	sp = leftnode(STAR, gp, gp->t_type, gp->t_size);
	gp->t_type = LPTR;
	gp->t_size = 0;
	*tpp = sp;
	return (1);
}

TREE *
modcall(tp, c)
register TREE *tp;
{
	tp->t_lp = modtree(tp->t_lp, MLADDR, tp);
	if (poolcallee(&tp->t_lp))
		walk(tp->t_lp, amd);
	tp->t_rp = modargs(tp->t_rp, tp);

	/*
	 * An indirect callee whose function-pointer LOAD address is not a
	 * single collectible operand (fns[i], tab[i].fn, p->q->fn) cannot
	 * be rendered as a CALL address -- gencoll cannot encode it.  Bind the
	 * pointer value into a stack temp and call through the temp,
	 * sequenced with COMMA exactly like the blkmv spill above.
	 */
	if (tp->t_lp != NULL && tp->t_lp->t_op == STAR
	 && calleehard(tp->t_lp->t_lp)) {
		register TREE	*proto, *ptmp, *asg, *cm;
		int		pt;

		pt = LPTR;
#if !ONLYSMALL
		if (!isvariant(VLARGE))
			pt = SPTR;
#else
		pt = SPTR;
#endif
		proto = makenode(GID, pt);
		proto->t_size = pertype[pt].p_size;
		ptmp = mdtempnode(proto);		/* *(FP-n) */
		asg = leftnode(ASSIGN, copynode(ptmp), pt, proto->t_size);
		asg->t_rp = tp->t_lp->t_lp;		/* tmp = the fn POINTER value */
		tp->t_lp->t_lp = copynode(ptmp);	/* call through the temp */
		cm = leftnode(COMMA, asg, tp->t_type, tp->t_size);
		cm->t_rp = tp;
		return (cm);
	}
	if (tp->t_type == BLK) {
#if !ONLYSMALL
		if (isvariant(VLARGE))
			tp->t_type = LPTR;
		else
			tp->t_type = SPTR;
#else
		tp->t_type = SPTR;
#endif
		if (c != MEFFECT)
			tp = leftnode(STAR, tp, BLK, tp->t_size);
	}
	return (tp);
}

/*
 * Modify argument lists.
 */
TREE *
modargs(tp, ptp)
register TREE	*tp;
TREE		*ptp;
{
	if (tp == NULL)
		return (NULL);
	if (tp->t_op == ARGLST) {
		tp->t_lp = modargs(tp->t_lp, tp);
		tp->t_rp = modargs(tp->t_rp, tp);
		return (tp);
	}
	if (tp->t_type == BLK) {
#if !ONLYSMALL
		tp = leftnode(ADDR, tp,
			(isvariant(VLARGE))?LPTB:SPTB,
			tp->t_size);
#else
		tp = leftnode(ADDR, tp, SPTB, tp->t_size);
#endif
		return (modtree(tp, MRVALUE, ptp));
	}
	return (modtree(tp, MFNARG, ptp));
}

/*
 * Given a pointer to a TREE node that describes an operation
 * that the machine cannot directly perform,
 * rewrite the node as a CALL to a magic routine.
 * On the iAPX-86 we rewrite floating point, long multiply and divide
 * and unsigned long multiply and divide.
 * This routine only has to handle the ASSIGN
 * operation if IEEE format; when using DECVAX format
 * the double=>float conversion is easy.
 */
TREE *
modxfun(tp)
TREE *tp;
{
	register TREE	*lp, *rp;
	register char	*p1, *p2;
	register TREE	*tp1;
	register int	tt, lt, op;
	register int	nptct, nptdt;

	static	char	*name[] = {
		"add",
		"sub",
		"mul",
		"div",
		"rem"
	};

	nptdt = SPTR;
#if !ONLYSMALL
	if (isvariant(VLARGE))
		nptdt = LPTR;
#endif
	tp1 = makenode(GID, nptdt);
	op  = tp->t_op;
	lp  = tp->t_lp;
	rp  = tp->t_rp;
	tt  = tp->t_type;
	if (lp != NULL)
		lt  = lp->t_type;
	p1  = id;
	if (op==NEG) {
		p2 = "neg";
		*p1++ = modoptab[tt];
		*p1++ = modoptab[lt];
	} else if (op==CONVERT || op==CAST) {
		/* Z8001 soft-float conversion names (libc/crt):
		 *   float<->double : dfpack / fdpack  (mantissa repack)
		 *   int->double    : d<src>flt        (diflt duflt dlflt dvflt)
		 *   double->int    : <dst>fix         (ifix ufix lfix vfix) */
		if (tt==F64 && lt==F32) {
			p2 = "pack"; *p1++ = 'd'; *p1++ = 'f';
		} else if (tt==F32 && lt==F64) {
			p2 = "pack"; *p1++ = 'f'; *p1++ = 'd';
		} else if (isflt(tt)) {
			p2 = "flt"; *p1++ = 'd'; *p1++ = modoptab[lt];
		} else {
			p2 = "fix"; *p1++ = modoptab[tt];
		}
	} else {
		walk(tp, amd);
		/* relop result is int, but the helper is named for the OPERAND type
		 * (double compare = dlcmp/drcmp, not ilcmp) -- use lt for relops. */
		*p1++ = isrelop(op) ? modoptab[lt] : modoptab[tt];
		/*
		 * Commute a leaf-left/nonleaf-right ADD/MUL/relop so the harder
		 * subtree is on the left.  BUT for pointer arithmetic keep the
		 * POINTER on the left: `p + i' must not flip to `i + p', because
		 * the segmented pointer+int rule (add.t) is pointer-left (offset
		 * added to the pointer's offset half, segment carried through).
		 * Otherwise selection has no pointer-left match and selfix loops
		 * materializing the index ("more than N stores").
		 */
		if ((op==ADD || op==MUL || isrelop(op))
		&& !(op==ADD && ispoint(lt))
		&& ((lp->t_flag&T_LEAF)!=0 && (rp->t_flag&T_LEAF)==0
		||   lp->t_op==DCON && rp->t_op!=DCON)) {
			lp = rp;
			rp = tp->t_lp;
			if (isrelop(op))
				op = fliprel[op-EQ];
		}
		if (op==ASSIGN || (op>=AADD && op<=AREM)) {
			*p1++ = modoptab[lt];
			if (lp->t_op != STAR)
				lp = leftnode(ADDR, lp, nptdt);
			else
				lp = lp->t_lp;
			if (op == ASSIGN)
				p2 = "asg";
			else
				p2 = name[op-AADD];
		} else if (isrelop(op))
			p2 = "cmp"; 
		else if (op>=ADD && op<=REM)
			p2 = name[op-ADD];
		else
			cbotch("modxfun");
		storedcon(rp);
		if (uselvalueform(op, tt, rp)) {
			*p1++ = 'l';
			if (rp->t_op != STAR)
				rp = leftnode(ADDR, rp, nptdt);
			else
				rp = rp->t_lp;
		} else
			*p1++ = 'r';
	}
	while (*p1++ = *p2++)
		;
	*p1 = 0;
	tp1->t_sp = gidpool(id);
	tp1->t_seg = SANY;
	lp = leftnode(ARGLST, lp, nptdt);
	lp->t_rp = rp;
	tp->t_op = CALL;
	tp->t_lp = tp1;
	tp->t_rp = lp;
	fixtoptype(tp);
	if (tp->t_type!=tt && tt!=F32)
		tp = leftnode(CONVERT, tp, tt);
	if (isrelop(op)) {
		tp = leftnode(op, tp, TRUTH);
		tp->t_rp = ivalnode(0);
	}
	return (tp);
}

/*
 * Zap a DCON into a block of memory with a double in it.
 */
storedcon(tp)
register TREE	*tp;
{
	if (tp->t_op != DCON)
		return;
	pool(tp);
	tp->t_flag = T_DIR;
}

/*
 * This routine, used only by the "modxfun" routine,
 * checks if an lvalue form of an operator routine can be used.
 * True return if it can.
 */
uselvalueform(op, tt, rp)
register TREE *rp;
{
	if (isrelop(op)) {
		if (rp->t_type != F64)
			return (0);
	} else {
		if (rp->t_type !=  tt)
			return (0);
	}
	if (rp->t_op==STAR || (rp->t_flag&T_DIR)!=0)
		return (1);
	return (0);
}

/*
 * Test if 1) the tree pointed to by "tp" is a register
 * and     2) the operation "op" can be computed in it.
 */
isokareg(tp, op)
register TREE *tp;
register op;
{
	register TREE *ap;

	if (op==MUL || op==DIV || op==REM)
		return (0);
	if (tp->t_op==REG && isword(tp->t_type))
		return (1);
	/*
	 * `(p = e) + n' on a FAR POINTER, hoisted to `(p = e, p + n)'.  Left whole, the
	 * sum's left operand is an assignment, which selection has to materialize into a
	 * pair of its own before the offset add; read back instead, the sum's base is the
	 * stored pointer itself.  The target has to be a simple lvalue leaf, so reading it
	 * back after the store costs nothing and repeats no side effect; malloc's guard,
	 * `(char *)(ap = sbrk(0)) + len <= (char *)ap', is the shape this is for.
	 */
	if (tp->t_op==ASSIGN && ispoint(tp->t_type) && islong(tp->t_type)
	 && (ap = tp->t_lp) != NULL
	 && (ap->t_op==REG || ap->t_op==AID || ap->t_op==PID || ap->t_op==LID))
		return (1);
	return (0);
}

/*
 * Deep-copy a subtree.  copynode() is shallow (it shares the children), so a
 * subtree that must appear in two places each of which selection will consume
 * needs a full copy.  Leaves (op < MIOBASE) hold a value, not tree children;
 * FIELD keeps its base/width in the t_rp slot rather than a child -- mirror the
 * traversal in walk1().
 */
TREE *
dupnode(tp)
register TREE *tp;
{
	register TREE *np;

	if (tp == NULL)
		return (NULL);
	np = copynode(tp);
	if (!isleaf(tp->t_op)) {
		np->t_lp = dupnode(tp->t_lp);
		if (tp->t_op != FIELD && tp->t_rp != NULL)
			np->t_rp = dupnode(tp->t_rp);
	}
	return (np);
}

/*
 * Modify bit fields in lvalue contexts.
 * The "tp" argument is a pointer to the tree node
 * with the FIELD operation on the left side.
 * This routine has two tasks.
 * First, it rewrites the type in the FIELD node to be the type used by
 * iselect to match the tree; this is also used as a flag
 * to prevent this routine from being called twice on a node.
 * Second, it inserts explict shift nodes to
 * the operands and results so that all of the optimizations
 * applied to shifts work for fields.
 * A pointer to the new top of the tree is returned.
 */
TREE *
modlfld(tp, c)
register TREE *tp;
{
	register TREE *lp, *rp;
	register lt, tt;
	register op;
	register bmop;
	register MASK mask;

	op = tp->t_op;
	tt = tp->t_type;
	lp = tp->t_lp;
	if (lp != NULL)
		lt = lp->t_type;
	/*
	 * A bitfield ++/-- has no dedicated FLD selection rule (a
	 * Z8000 INC/DEC on the machine object would clobber the neighbour bits).
	 * A PREFIX ++/-- -- and an EFFECT-context postfix, which modtree has already
	 * folded into the prefix op -- rewrites to the masked compound-assign
	 * `field += 1' / `field -= 1' below, reusing the field load-modify-store.
	 *
	 * A VALUE-context postfix (`y = field++') still needs the OLD field value,
	 * which the compound-assign cannot supply: its value is the NEW field, and
	 * `(field += 1) - 1' would mis-read across the modular wrap.  Lower it to
	 *	(tmp = field, field += 1, tmp)
	 * reading the old value into a temp first.  The field appears twice (the
	 * read and the increment) and selection consumes trees destructively, so
	 * the read runs on a deep copy of the FIELD subtree.
	 */
	if ((op==INCAFT || op==DECAFT) && c != MEFFECT) {
		register TREE *fld, *oldv, *tmp, *proto, *cm, *res;
		register int vt;

		fld = dupnode(lp);			/* private copy for the read */
		oldv = modefld(fld->t_lp, fld, MRVALUE, 1);
		if (isbyte(oldv->t_type)) {		/* hold a byte field's value in a word temp */
			oldv = leftnode(CONVERT, oldv, oldv->t_type);
			fixtoptype(oldv);
		}
		vt = oldv->t_type;
		proto = makenode(GID, vt);
		proto->t_size = pertype[vt].p_size;
		tmp = mdtempnode(proto);
		cm = leftnode(ASSIGN, copynode(tmp), vt, proto->t_size);
		cm->t_rp = oldv;			/* tmp = field (old value) */
		tp->t_op = (op==INCAFT) ? AADD : ASUB;
		tp->t_rp = ivalnode((ival_t)1);
		tp = modlfld(tp, MEFFECT);		/* field += 1 (for effect) */
		res = leftnode(COMMA, cm, vt, proto->t_size);
		res->t_rp = leftnode(COMMA, tp, vt, proto->t_size);
		res->t_rp->t_rp = copynode(tmp);	/* (tmp=old, field+=1, tmp) */
		return (res);
	}
	if (op == INCBEF || op == DECBEF) {
		tp->t_op = op = (op == INCBEF) ? AADD : ASUB;
		tp->t_rp = ivalnode((ival_t)1);
	}
	if (op!=AMUL && op!=ADIV && op!=ASHL && op!=ASHR) {
		rp = tp->t_rp;
		rp = leftnode(SHL, rp, rp->t_type);
		rp->t_rp = ivalnode(lp->t_base);
		tp->t_rp = rp;
	}
	if (op==AAND || op==AOR || op==AXOR || op==ASSIGN) {
		mask = ((MASK)01<<lp->t_width) - 1;
		mask = mask << lp->t_base;
		bmop = AND;
		if (op == AAND) {
			mask = ~mask;
			bmop = OR;
		}
		/* rp set above */
		rp = leftnode(bmop, rp, rp->t_type);
		rp->t_rp = ivalnode(mask);
		tp->t_rp = rp;
	}
	if (c != MEFFECT)
		tp = modefld(tp, lp, c, 0);
	if (isbyte(lt))
		lp->t_type = FLD8;
	else
		lp->t_type = FLD16;
	return (tp);
}

/*
 * This function rewrites any field extraction.
 * The argument "tp" is the base of the field.
 * The argument "fp" is a FIELD node that supplies the width and the base.
 * The argument "flag" is true to enable the mask off in unsigned field extract.
 */
TREE *
modefld(tp, fp, c, flag)
register TREE *tp;
register TREE *fp;
{
	register n;
	register tt;
	register mw;
	register mask;
	register ttold;

	mw = 16;
	if (isbyte(ttold = tt = tp->t_type)) {
		/*
		 * A byte field is promoted to a word register (the CONVERT
		 * below) before the extract shifts, because the Z8001 shift
		 * rules used here are word ops.  The signed extraction shifts
		 * therefore run in a 16-bit register -- keep mw = 16 so the
		 * field's sign bit lands in bit 15 (mw = 8 would sign-extend
		 * from the byte's bit 7, not the field's, and read garbage).
		 */
		tp = leftnode(CONVERT, tp, tt);
		fixtoptype(tp);
		tt = tp->t_type;
	}
	if (c == MFLOW) {
		mask = (01<<fp->t_width) - 1;
		if ((n=fp->t_base) != 0)
			mask <<= n;
		tp = leftnode(AND, tp, tt);
		tp->t_rp = ivalnode((ival_t)mask);
		return (tp);
	}
	if (isuns(fp->t_type)) {
		if ((n=fp->t_base) != 0) {
			tp = leftnode(SHR, tp, tt);
			tp->t_rp = ivalnode(n);
		}
		if (flag && (n=fp->t_width)<mw) {
			tp = leftnode(AND, tp, tt);
			tp->t_rp = ivalnode(((ival_t)01<<n)-1);
		}
		return (tp);
	}
	if ((n=mw-(fp->t_base+fp->t_width)) != 0) {
		tp = leftnode(SHL, tp, tt);
		tp->t_rp = ivalnode(n);
	}
	if ((n=mw-fp->t_width) != 0) {
		tp = leftnode(SHR, tp, tt);
		tp->t_rp = ivalnode(n);
	}
	if (ttold != tt)
		tp = leftnode(CONVERT, tp, ttold);
	return (tp);
}

/*
 * Check if a tree should have its left and right subtrees swapped.
 * Do it if it is required.
 * Sometimes the relational operation must be adjusted.
 */
modswap(tp, ptp)
register TREE	*tp;
TREE		*ptp;
{
	register TREE *lp, *rp;
	FLAG lf, rf;

	switch (tp->t_op) {

	case ADD:
		/* Far-pointer index (`p[i]', `p+i', `p-k'), which cc0 spells either way round:
		 * the POINTER goes LEFT and stays there, where add.t's pointer-LEFT rule adds the
		 * index into the offset half in ONE instruction and carries the segment half
		 * across.  The generic flag rule below would move it right for the two-instruction
		 * int-LEFT form; isptrleft names the two pointer forms that do want to be right
		 * (a static base, for X-mode addressing, and the frame register, for the near
		 * `int + FP' rule) and both fall through to it.  A sum of two far pointers is not
		 * an index and falls through as well. */
		if (isfarptr(tp->t_lp) && !isfarptr(tp->t_rp) && isptrleft(tp->t_lp))
			return;
		if (isfarptr(tp->t_rp) && !isfarptr(tp->t_lp) && isptrleft(tp->t_rp)) {
			swapit(tp);
			return;
		}
	case MUL:
	case AND:
	case OR:
	case XOR:
	case EQ:
	case NE:
	case GT:
	case GE:
	case LT:
	case LE:
	case UGT:
	case UGE:
	case ULT:
	case ULE:
		lp = tp->t_lp;
		rp = tp->t_rp;
		lf = lp->t_flag;
		rf = rp->t_flag;
		if (lf!=0 || rf!=0) {
			if ((lf&T_CON) != 0
			||  (rf&T_REG) != 0
			||  (lf!=0 && rf==0))
				swapit(tp);
		} else if (lp->t_cost > rp->t_cost)
			swapit(tp);
	}
}
	
/*
 * Swap subtrees.
 * Fix up relational ops.
 */
swapit(tp)
register TREE	*tp;
{
	register TREE *xp;
	register op;

	xp = tp->t_lp;
	tp->t_lp = tp->t_rp;
	tp->t_rp = xp;
	op = tp->t_op;
	if (isrelop(op))
		tp->t_op = fliprel[op-EQ];
}

/*
 * Convert integer constant val to type t.
 */
lval_t
constcvt(t, val) register int t; lval_t val;
{
	if (isbyte(t))
		val &= 0xFFL;
	else if (isword(t))
		val &= 0xFFFFL;
	if (!isuns(t)) {
		if (isbyte(t) && (val & 0x80L) != 0)
			val |= 0xFFFFFF00L;
		else if (isword(t) && (val & 0x8000L) != 0)
			val |= 0xFFFF0000L;
	}
	return val;
}

/* end of n1/i8086/mtree2.c */
