/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * lexlib.c - the run-time half of lex(1): the NFA simulator that the
 * generated lex.yy.c drives.  The generator (cmd/lex) writes yylex() as
 * a switch over _lltk(), the rule number of the next token, and emits
 * these tables:
 *
 *	int yy_lextab[]		the automaton, one int per state:
 *				op in the low LR_SHFT bits, data above
 *	int yy_ctxtab[]		start state of each context (0 = default)
 *	int yy_lxctab[]		character classes, 256 bits each as
 *				(256/NBINT) ints; yy_lxbtab[] the bit masks
 *	int yy_clist[], ..	list storage the generator sizes; unused here
 *
 * and expects from the library: yytext[], yyleng, yyscon, yyline,
 * _lltk(), yyback() (unput), yyrjct() (REJECT), yyctxt() (yyswitch).
 * Input comes through the generated _llic() (input(), i.e. getchar())
 * and unmatched text goes out through _lloc(c) (output()).
 *
 * State semantics, from the generator's rexparse/nfalink:
 *	LINK +n		epsilon fork: this state continues at s+1 AND s+n
 *	JUMP +n		epsilon goto s+n
 *	CHAR c		consume c (c == -1 is end of file), then s+1
 *	CLAS n		consume a character of class n, then s+1
 *	ANYC		consume any character but newline, then s+1
 *	BLIN		only at the start of a line (^), then s+1
 *	ELIN		only if the next character is a newline ($), s+1
 *	SCON n		only in start condition n (BEGIN n), then s+1
 *	LOOK		trailing context (/): the token ends here, s+1
 *	ACPT n		rule n matches the text so far
 *	STOP		end of a context's rule list
 * Each rule starts with a LINK to the next rule, so a context is one
 * NFA whose start state is yy_ctxtab[context].  The simulator keeps a
 * set of live states, feeds each input character to all of them, and
 * remembers the longest accepting position (earliest rule on a tie),
 * as lex always has.  REJECT falls back to the next candidate of the
 * same token in that order.
 */
#include <stdio.h>
#include "../../cmd/lex/lextype.h"

#define	YYLMAX	1024		/* longest token */
#define	PBSIZE	1024		/* pushback (unput and read-ahead) */
#define	NSTATE	1000		/* ARRSZ in lex.h: largest automaton */
#define	NBINT	16		/* bits per int, as the generator's tables */
#define	NCAND	64		/* accepting candidates kept for REJECT */

extern	int	yy_lextab[];
extern	int	yy_ctxtab[];
extern	int	yy_lxctab[];
extern	int	yy_lxbtab[];

char	yytext[YYLMAX+1];
int	yyleng;
int	yyscon;			/* start condition, set by BEGIN */
int	yyline = 1;		/* input line number */

static	int	yyctx;		/* current context, set by yyswitch() */
static	char	pbbuf[PBSIZE];	/* pushed-back characters, last first */
static	int	pbn;
static	int	lastc = '\n';	/* last character consumed, for ^ */

/* the live-state sets: state number and the LOOK position of its thread */
static	short	cstate[NSTATE], clook[NSTATE];
static	short	nstate[NSTATE], nlook[NSTATE];
static	short	stamp[NSTATE];	/* generation mark, one entry per state */
static	short	gen;

/* the scan buffer of the current token and its accepting candidates */
static	char	buf[YYLMAX+1];
static	int	nbuf;
static	struct	cand { short rule, len; } cand[NCAND];
static	int	ncand, curcand;
static	int	rejecting;

#define	OP(s)	(yy_lextab[s] & LR_MASK)
#define	DATA(s)	(yy_lextab[s] >> LR_SHFT)

static	int	getin();
static	void	pushback();
static	int	inclass();
static	void	addstate();
static	void	addcand();
static	void	sortcand();

/*
 * Find the next token: scan from the current position, return the
 * number of the rule that matched (yytext/yyleng set), 0 at end of
 * input.  Text no rule matches is copied to the output a character at
 * a time, as lex does.
 */
_lltk()
{
	register int c, i, n, s, op;
	register struct cand *cp;

	if (rejecting) {
		/* REJECT: take the next candidate for the same starting
		 * position.  buf[0..nbuf) still holds the accepted text and
		 * everything past it has been pushed back, so the accept
		 * code below only has to give back the difference. */
		rejecting = 0;
		if (++curcand < ncand)
			goto accept;
		/* nothing else matched here: the default action */
		while (nbuf > 1)
			pushback(buf[--nbuf]);
		c = buf[0];
		nbuf = 0;
		goto dflt;
	}
again:
	nbuf = 0;
	ncand = 0;
	curcand = 0;
	n = 0;
	gen++;
	addstate(yy_ctxtab[yyctx], -1, cstate, clook, &n, 0);
	while (n > 0) {
		if (nbuf >= YYLMAX)
			break;
		c = getin();
		if (c == EOF) {
			/* only CHAR EOF states can go on */
			i = 0;
			for (s = 0; s < n; s++)
				if (OP(cstate[s]) == LX_CHAR && DATA(cstate[s]) == -1) {
					gen++;
					addstate(cstate[s]+1, clook[s], nstate, nlook, &i, nbuf);
				}
			n = 0;
			for (s = 0; s < i; s++) {
				cstate[s] = nstate[s];
				clook[s] = nlook[s];
			}
			n = i;
			continue;
		}
		buf[nbuf++] = c;
		gen++;
		i = 0;
		for (s = 0; s < n; s++) {
			op = OP(cstate[s]);
			if ((op == LX_CHAR && DATA(cstate[s]) == c)
			 || (op == LX_CLAS && inclass(DATA(cstate[s]), c))
			 || (op == LX_ANYC && c != '\n'))
				addstate(cstate[s]+1, clook[s], nstate, nlook, &i, nbuf);
		}
		if (i == 0)		/* no state survives: the scan is over; the
					 * accept code below gives back whatever lies
					 * past the chosen candidate, this character
					 * included when nothing matched it */
			break;
		for (s = 0; s < i; s++) {
			cstate[s] = nstate[s];
			clook[s] = nlook[s];
		}
		n = i;
	}
	if (ncand == 0) {
		if (nbuf == 0)
			return (0);	/* end of input */
		/* no rule matches here: copy one character through */
		while (nbuf > 1)
			pushback(buf[--nbuf]);
		c = buf[0];
		nbuf = 0;
dflt:
		if (c == '\n')
			yyline++;
		lastc = c;
		_lloc(c);
		goto again;
	}
	sortcand();		/* choose the winner; order the rest for REJECT */
accept:
	cp = &cand[curcand];
	while (nbuf > cp->len)		/* read-ahead beyond the token */
		pushback(buf[--nbuf]);
	yyleng = cp->len;
	for (i = 0; i < yyleng; i++) {
		yytext[i] = buf[i];
		if (buf[i] == '\n')
			yyline++;
	}
	yytext[yyleng] = '\0';
	if (yyleng > 0)
		lastc = yytext[yyleng-1];
	nbuf = cp->len;			/* what REJECT would give back */
	return (cp->rule);
}

/*
 * Add state s (and everything reachable from it without consuming
 * input) to the list, once per generation.  `look' is the position
 * where the thread passed a LOOK state, -1 if none; `pos' is the
 * number of characters consumed so far.
 */
static void
addstate(s, look, list, looks, np, pos)
register int s, look;
short *list, *looks;
int *np, pos;
{
	register int c;

	for (;;) {
		if (s < 0 || s >= NSTATE || stamp[s] == gen)
			return;
		stamp[s] = gen;
		switch (OP(s)) {
		case LX_STOP:
			return;
		case LX_LINK:
			addstate(s + DATA(s), look, list, looks, np, pos);
			s++;
			continue;
		case LX_JUMP:
			s += DATA(s);
			continue;
		case LX_SCON:
			if (yyscon != DATA(s))
				return;
			s++;
			continue;
		case LX_BLIN:
			if (pos != 0 || lastc != '\n')
				return;
			s++;
			continue;
		case LX_ELIN:
			c = getin();
			if (c != EOF)
				pushback(c);
			if (c != '\n' && c != EOF)
				return;
			s++;
			continue;
		case LX_LOOK:
			look = pos;
			s++;
			continue;
		case LX_ACPT:
			addcand(DATA(s), look >= 0 ? look : pos);
			return;
		case LX_CHAR:
		case LX_CLAS:
		case LX_ANYC:
			if (*np < NSTATE) {
				list[*np] = s;
				looks[*np] = look;
				(*np)++;
			}
			return;
		default:
			return;
		}
	}
}

/*
 * Record an accepting candidate: one entry per rule, holding the
 * longest position at which that rule accepts.  Kept UNSORTED --
 * sortcand() orders them at accept time, because the lengths arrive
 * out of order (a later rule can reach its final length before an
 * earlier rule reaches the same length) and an incremental sort here
 * could not keep "longest, then earliest rule" correct.
 */
static void
addcand(rule, len)
int rule, len;
{
	register int i;

	for (i = 0; i < ncand; i++)
		if (cand[i].rule == rule) {
			if (len > cand[i].len)
				cand[i].len = len;
			return;
		}
	if (ncand >= NCAND)
		return;
	cand[ncand].rule = rule;
	cand[ncand].len = len;
	ncand++;
}

/*
 * Order the candidates longest first, and on equal length by rule
 * number, so cand[0] is what lex chooses and the rest, in order, are
 * what REJECT falls back to.
 */
static void
sortcand()
{
	register int i, j;
	struct cand t;

	for (i = 1; i < ncand; i++) {
		t = cand[i];
		for (j = i; j > 0 && (cand[j-1].len < t.len ||
		    (cand[j-1].len == t.len && cand[j-1].rule > t.rule)); j--)
			cand[j] = cand[j-1];
		cand[j] = t;
	}
}

static
inclass(n, c)
int n, c;
{
	c &= 0377;
	return (yy_lxctab[n * (256/NBINT) + c/NBINT] & yy_lxbtab[c % NBINT]) != 0;
}

static
getin()
{
	if (pbn > 0)
		return (pbbuf[--pbn] & 0377);
	return (_llic());
}

static void
pushback(c)
int c;
{
	if (pbn < PBSIZE)
		pbbuf[pbn++] = c;
}

/*
 * unput(c): push a character back onto the input.
 */
yyback(c)
int c;
{
	if (c == '\n')
		yyline--;
	pushback(c);
}

/*
 * REJECT: the next _lltk() re-decides the current token.
 */
yyrjct()
{
	rejecting = 1;
}

/*
 * yyswitch(x): change the context (%x contexts of the specification).
 */
yyctxt(x, n)
int x, n;
{
	if (x < 0 || x >= n) {
		fprintf(stderr, "lex: bad context %d\n", x);
		exit(1);
	}
	yyctx = x;
}
