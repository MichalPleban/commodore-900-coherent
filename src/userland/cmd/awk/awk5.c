/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Awk - internal execution functions.
 */

#include "awk.h"
#include "y.tab.h"

FILE	*xoutput();

/*
 * `print' directive.
 * First argument is the NODE (or list) to print
 * and the second is the output.
 * Have to close pipes specially.
 * The ALIST stuff should be generalised
 * so that functions can get their arguments
 * a little more easily.
 */
xprint(np, xp)
register NODE *np;
register NODE *xp;
{
	register FILE *ofp;

	ofp = xoutput(xp);
	while (np != NULL) {
		if (np->n_op == ALIST) {
			xp = np->n_O1;
			np = np->n_O2;
		} else {
			xp = np;
			np = NULL;
		}
		xp = evalexpr(xp);
		if (xp->t_flag & T_NUM) {
			char nbuf[40];

			if (xp->t_flag & T_INT)
				fprintf(ofp, OFMT, xp->t_INT); else
				fprintf(ofp, "%s", numstr(xp, nbuf));
		} else
			fprintf(ofp, "%s", xp->t_STRING);
		if (np != NULL)
			fprintf(ofp, "%s", OFS);
	}
	fprintf(ofp, "%s", ORS);
	fflush(ofp);
}

/*
 * `printf' directive.
 * First argument is list, second
 * is output.
 * If third argument is non-NULL,
 * it is used for sprintf rather than
 * printf.
 */
xprintf(np, xp, sp)
NODE *np;
NODE *xp;
STRING sp;
{
	NODE *nextarg();
	int *pflist;
	register char *cp;
	register int *pflp;
	register int c;
	register int i;
	register FILE *ofp;

	/* One slot per conversion in the FORMAT (plus the format and a spare):
	 * the number of arguments is no measure of what gets stored here --
	 * six %s with one argument used to write past the block. */
	cp = evalstring(nextarg(np, 1));
	{
		register char *fp;
		int nconv;

		for (fp = cp, nconv = 2; *fp != '\0'; fp++)
			if (*fp == '%')
				nconv += 2;	/* a * width and the value */
		pflp = pflist = (int *)xalloc(nconv * sizeof(double));
	}
	if (sp == NULL)
		ofp = xoutput(xp); else
		*sp = '\0';
	i = 2;
	*((char **)pflp) = cp;
	bump(pflp, char*);
	for (;;) {
		while ((c = *cp++)!='%' && c!='\0')
			;
		if (c == '\0')
			break;
		if (*cp == '-')
			cp++;
		if (*cp == '*') {
			*pflp++ = evalint(nextarg(np, i++));
			cp++;
		} else
			while (isdigit(*cp))
				cp++;
		if (*cp == '.') {
			cp++;
			if (*cp == '*') {
				*pflp++ = evalint(nextarg(np, i++));
				cp++;
			} else
				while (isdigit(*cp))
					cp++;
		}
		if ((c = *cp++) == 'l')
			c = toupper(*cp++);
		switch (c) {
		case 'd':
		case 'u':
		case 'x':
		case 'o':
			*pflp++ = evalint(nextarg(np, i++));
			break;

		case 'D':
		case 'U':
		case 'X':
		case 'O':
			*((long *)pflp) = (long)evalint(nextarg(np, i++));
			bump(pflp, long);
			break;

		case 'e':
		case 'f':
		case 'g':
			*((double *)pflp) = (double)evalfloat(nextarg(np, i++));
			bump(pflp, double);
			break;

		case 'c':
			xp = evalexpr(nextarg(np, i++));
			if (xp->n_flag & T_NUM)
				*pflp++ = evalint(xp); else
				*pflp++ = *evalstring(xp);
			break;

		case 's':
			*((char **)pflp) = evalstring(nextarg(np, i++));
			bump(pflp, char*);
			break;

		case 'r':
			awkwarn("%%r not available in sprintf/printf");
			break;
		}
	}
	if (sp == NULL) {
		fprintf(ofp, "%r", pflist);
		fflush(ofp);
	} else
		sprintf(sp, "%r", pflist);
	free(pflist);
}

/*
  * Return the next argument for printf.
 */
static NODE *
nextarg(anp, n)
register NODE *anp;
register int n;
{
	/* fargn() hands back the LAST argument once the list runs out; a
	 * conversion without an argument gets an empty string (0) instead. */
	if (n > fnargs(anp) || (anp = fargn(anp, n)) == NULL)
		return (snode(SNULL, 0));
	return (anp);
}

/*
 * Calculate the output
 * stream for print or printf.
 * This saves up names so that they
 * don't get re-opened every time.
 */
FILE *
xoutput(np)
register NODE *np;
{
	register char *s;
	register OFILE *ofp;
	register OFILE *ofslot;

	if (np == NULL)
		return (stdout);
	s = evalstring(np->n_O1);
	ofslot = NULL;
	for (ofp = files; ofp < endof(files); ofp++)
		if (ofp->of_fp != NULL) {
			if (strcmp(ofp->of_name, s) == 0)
				return (ofp->of_fp);
		} else
			ofslot = ofp;
	if ((ofp = ofslot) == NULL)
		awkerr("Too many output files or pipes");
	ofp->of_flag = 0;
	switch (np->n_op) {
	case AFOUT:
		if ((ofp->of_fp = fopen(s, "w")) == NULL)
			awkerr("Cannot open output `%s'", s);
		break;

	case AFAPP:
		if ((ofp->of_fp = fopen(s, "a")) == NULL)
			awkerr("Cannot open `%s' for append", s);
		break;

	case AFPIPE:
		if ((ofp->of_fp = popen(s, "w")) == NULL)
			awkerr("Cannot create pipe to `%s'", s);
		ofp->of_flag = OFPIPE;
		break;

	default:
		awkerr("Bad output tree op %d", np->n_op);
	}
	ofp->of_name = xalloc(strlen(s) + sizeof(char));
	strcpy(ofp->of_name, s);
	setbuf(ofp->of_fp, outbuf);
	return (ofp->of_fp);
}

/*
 * Do the form: for (i in array) stat
 * `var' is the index and `stat' the statement.
 */
xforin(var, array, stat)
NODE *var;
register NODE *array;
NODE *stat;
{
	register char *cp;
	register TERM *tp;
	register int i;
	register int j;

	for (i=0; i<NHASH; i++)
		for (tp = symtab[i]; tp != NULL; tp = tp->t_next)
			if (tp->t_ahval==array->t_hval && tp->t_flag&T_ARRAY
			    && streq(tp->t_name, array->t_name)) {
				if ((j = setjmp(fwenv[fwlevel])) == ABREAK)
					break;
				else if (j == ACONTIN)
					continue;
				cp = tp->t_name;
				while (*cp++ != '\0')
					;
				xassign(var, snode(cp, 0));
				evalact(stat);
			}
}

/*
 * Return a node associated with
 * an array element.
 * `array' is the array identifier,
 * and `index' is the index expression
 * represented as a STRING.
 */
NODE *
xarray(array, index)
NODE *array;
NODE *index;
{
	return (alookup(array->t_name, evalstring(index)));
}

/*
 * The fields of the current record: split ONCE, on first use, into
 * `fldv' (fldn strings) and kept until the next record.  Reading a
 * field copies one out; assigning a field, $0 or NF changes the array
 * and re-joins the record with OFS -- what every awk does.  (Splicing
 * the new text into the old record kept the original separators, and
 * re-splitting on every access then lost the assigned fields.)
 */
static
fldadd(p, n)
register char *p;
register int n;
{
	register char *q;

	if (fldn >= fldmax) {
		register int j;
		char **nv;

		nv = (char **)xalloc((unsigned)(fldmax + 32) * sizeof(char *));
		for (j = 0; j < fldn; j++)
			nv[j] = fldv[j];
		if (fldv != NULL)
			free((char *)fldv);
		fldv = nv;
		fldmax += 32;
	}
	q = xalloc((unsigned)n + sizeof(char));
	fldv[fldn++] = q;
	while (n-- > 0)
		*q++ = *p++;
	*q = '\0';
}

static
fldsplit()
{
	register unsigned char *s, *p;
	register int c;

	while (fldn > 0)
		free(fldv[--fldn]);
	s = (unsigned char *)inline;
	if (!fsblank) {
		if (*s != '\0')
			for (;;) {
				p = s;
				while ((c = *s) != '\0' && !FSMAP[c])
					s++;
				fldadd((char *)p, (int)(s - p));
				if (c == '\0')
					break;
				s++;
			}
	} else for (;;) {
		while (FSMAP[*s])
			s++;
		if (*s == '\0')
			break;
		p = s;
		while ((c = *s) != '\0' && !FSMAP[c])
			s++;
		fldadd((char *)p, (int)(s - p));
	}
	fldvalid = 1;
}

/*
 * Make `rec' the current record, freeing the one it replaces unless
 * that is the input buffer itself (execute() owns that one).
 */
static
setrec(rec)
char *rec;
{
	if (inline != NULL && inline != inrec)
		free(inline);
	inline = rec;
}

/* Join fldv with OFS into a new record, make it current and set NF. */
static
fldjoin()
{
	register int j;
	register char *p, *q;
	register unsigned nb;
	char *rec;

	nb = sizeof(char);
	for (j = 0; j < fldn; j++)
		nb += strlen(fldv[j]) + strlen(OFS);
	rec = q = xalloc(nb);
	for (j = 0; j < fldn; j++) {
		if (j != 0)
			for (p = OFS; *p != '\0'; )
				*q++ = *p++;
		for (p = fldv[j]; *p != '\0'; )
			*q++ = *p++;
	}
	*q = '\0';
	setrec(rec);
	iassign(NFp, (INT)fldn);
}

/* NF was assigned: cut the record to n fields, or pad it with empty ones. */
fldsetnf(n)
int n;
{
	if (n < 0 || n > MAXNF)
		awkerr("Bad field count NF = %d", n);
	if (!fldvalid)
		fldsplit();
	while (fldn > n)
		free(fldv[--fldn]);
	while (fldn < n)
		fldadd("", 0);
	fldjoin();
}

/*
 * Extract the field given by the expression.
 * A negative field number is
 * considered to be from the end.
 * The `asval' is non-NULL when
 * the string is to be assigned to a field.
 */
NODE *
xfield(i, asval)
int i;
STRING asval;
{
	register char *as;

	if (inline == NULL) {
		awkwarn("field, $%d, illegal in BEGIN or END", i);
		return (snode(SNULL, 0));
	}
	if (i == 0) {
		if (asval != NULL) {
			as = xalloc(strlen(asval)+sizeof(char));
			strcpy(as, asval);
			setrec(as);
			fldvalid = 0;
			iassign(NFp, (INT)countnf(inline));
		}
		return (snode(inline, T_STRNUM));
	}
	if (i < 0)
		i += (int)NF + 1;
	if (!fldvalid)
		fldsplit();
	if (asval != NULL) {
		if (i < 1 || i > MAXNF)
			awkerr("Bad field number $%d", i);
		while (fldn < i)
			fldadd("", 0);
		as = xalloc(strlen(asval)+sizeof(char));
		strcpy(as, asval);
		free(fldv[i-1]);
		fldv[i-1] = as;
		fldjoin();
		return (snode(inline, T_STRNUM));
	}
	if (i < 1 || i > fldn)
		return (snode(SNULL, 0));
	as = xalloc(strlen(fldv[i-1])+sizeof(char));
	strcpy(as, fldv[i-1]);
	return (snode(as, T_ALLOC|T_STRNUM));
}

/*
 * Assignment of fields support.
 * The arguments are:
 * `f1', `f2', `middle', `e1', `e2'
 * for the front start and stop, the middle
 * and the end start and stop, respectively.
 */
char *
xfield1(f1, f2, middle, e1, e2)
char *f1, *f2;
char *middle;
char *e1, *e2;
{
	register char *p1, *p2;
	register char *as;

	as = xalloc(f2-f1 + e2-e1 + strlen(middle) + sizeof(char));
	p1 = as;
	p2 = f1;
	while (p2 < f2)
		*p1++ = *p2++;
	p2 = middle;
	while (*p2 != '\0')
		*p1++ = *p2++;
	p2 = e1;
	while (p2 < e2)
		*p1++ = *p2++;
	*p1 = '\0';
	return (as);
}

/*
 * String catenation in two nodes.
 */
NODE *
xconc(n1, n2)
register NODE *n1, *n2;
{
	register char *ap;
	register char *cp1, *cp2;
	register int n;

	n = strlen(ap = evalstring(n1)) + sizeof(char);
	if ((n1->t_flag & T_NUM) == 0) {
		cp1 = xalloc(n);
		strcpy(cp1, ap);
	} else
		cp1 = ap;
	n += strlen(cp2 = evalstring(n2));
	ap = xalloc(n);
	strcpy(ap, cp1);
	strcat(ap, cp2);
	if ((n1->t_flag & T_NUM) == 0)
		free(cp1);
	return (snode(ap, T_ALLOC));
}

/*
 * Arithmetic operations --
 *
 * Numeric addition
 */
NODE *
xadd(n1, n2)
register NODE *n1, *n2;
{
	if (isfloat(n1) || isfloat(n2))
		return (fnode(evalfloat(n1) + evalfloat(n2)));
	return (intres((FLOAT)evalint(n1) + (FLOAT)evalint(n2),
	    evalint(n1) + evalint(n2)));
}

/*
 * Subtraction -- actually a numeric operation.
 */
NODE *
xsub(n1, n2)
register NODE *n1, *n2;
{
	if (isfloat(n1) || isfloat(n2))
		return (fnode(evalfloat(n1) - evalfloat(n2)));
	return (intres((FLOAT)evalint(n1) - (FLOAT)evalint(n2),
	    evalint(n1) - evalint(n2)));
}

/*
 * Multiplication
 */
NODE *
xmul(n1, n2)
register NODE *n1, *n2;
{
	if (isfloat(n1) || isfloat(n2))
		return (fnode(evalfloat(n1) * evalfloat(n2)));
	return (intres((FLOAT)evalint(n1) * (FLOAT)evalint(n2),
	    evalint(n1) * evalint(n2)));
}

/*
 * Division
 * If either numeric is of internal FLOAT type,
 * the division will be a float one, otherwise use
 * INT division.
 */
NODE *
xdiv(n1, n2)
register NODE *n1, *n2;
{
	register INT a, b;

	if (isfloat(n1) || isfloat(n2)) {
		FLOAT fa, fb;

		fa = evalfloat(n1);
		if ((fb = evalfloat(n2)) == 0.0)
			awkerr("Division by zero");
		return (fnode(fa / fb));
	}
	a = evalint(n1);
	if ((b = evalint(n2)) == 0)
		awkerr("Division by zero");
	if (a % b != 0)		/* inexact: awk arithmetic is real, 7/2 is 3.5 */
		return (fnode((FLOAT)a / (FLOAT)b));
	return (inode(a / b));
}

/*
 * Modulus
 * Same type conversion rule as for division.
 */
NODE *
xmod(n1, n2)
register NODE *n1, *n2;
{
	register INT a, b;

	if (isfloat(n1) || isfloat(n2))
		awkwarn("Modulus operator not allowed on floating point");
	a = evalint(n1);
	if ((b = evalint(n2)) == 0)
		awkerr("Division by zero");
	return (inode(a % b));
}

/*
 * Comparison operators --
 * string or numeric comparison
 * for equality or non-equality.
 * The tricks come in conversions
 * between FLOAT and INT.
 * The nodes passed should not be evaluated
 * beforehand so that checks for fields can
 * be made as here fields are always considered
 * as strings.
 */
NODE *
xcmp(n1, n2, op)
register NODE *n1, *n2;
int op;
{
	register int result;
	register int isnum = 0;

	if (n1->n_op != AFIELD)
		isnum = isnumeric(n1 = evalexpr(n1));
	else
		n1 = evalexpr(n1);
	if (n2->n_op != AFIELD)
		isnum |= isnumeric(n2 = evalexpr(n2));
	else
		n2 = evalexpr(n2);
	if (isnum) {
		result = 0;
		if (isfloat(n1) || isfloat(n2)) {
			register FLOAT f1, f2;

			if ((f1 = evalfloat(n1)) > (f2 = evalfloat(n2)))
				result++;
			else if (f1 < f2)
				result--;
		} else {
			register INT i1, i2;

			if ((i1 = evalint(n1)) > (i2 = evalint(n2)))
				result++;
			else if (i1 < i2)
				result--;
		}
	} else if ((n1->t_flag & T_NUM)==0 && (n2->t_flag & T_NUM)==0)
		result = strcmp(n1->t_STRING, n2->t_STRING);
	else
		result = strcmp(evalstring(n1), evalstring(n2));
	switch (op) {
	case AEQ:
		result = result==0;
		break;

	case ANE:
		result = result!=0;
		break;

	case AGT:
		result = result>0;
		break;

	case AGE:
		result = result>=0;
		break;

	case ALT:
		result = result<0;
		break;

	case ALE:
		result = result<=0;
		break;
	}
	return (inode((INT)result));
}

/*
 * Assignment
 * The two nodes `l' and `r' are the left
 * and right sides of the assignment,
 * respectively.
 */
NODE *
xassign(l, r)
register NODE *l, *r;
{

	if (l->t_op == AFIELD)
		return (xfield((int)evalint(l->n_O1), evalstring(r)));
	else if (l->t_op == AARRAY)
		l = xarray(l->n_O1, l->n_O2);
	if ((l->t_flag & (T_ALLOC|T_NUM)) == T_ALLOC)
		free(l->t_STRING);
	l->t_flag &= ~(T_INT|T_NUM|T_STRNUM);
	l->t_flag |= T_ALLOC|(r->t_flag & (T_INT|T_NUM|T_STRNUM));
	if (r->t_flag & T_NUM)
		if (r->t_flag & T_INT)
			l->t_INT = r->t_INT; else
			l->t_FLOAT = r->t_FLOAT;
	else {
		l->t_STRING = xalloc(strlen(r->t_STRING)+sizeof(char));
		strcpy(l->t_STRING, r->t_STRING);
	}
	if (l == FSp)
		fsmapinit(evalstring(l));
	else if (l == NFp && inline != NULL && !beginflag && !endflag)
		fldsetnf((int)evalint(l));		/* NF = n cuts or pads $0 */
	return (l);
}


/*
 * The number in `np' as a string: an integer, or a real that is a whole
 * number (2147483647 + 1, or 6 / 2), prints as one; anything else in
 * %.6g, as awk does.
 */
char *
numstr(np, buf)
register NODE *np;
char *buf;
{
	register FLOAT f;
	double ip;
	extern double modf();

	if (np->t_flag & T_INT)
		sprintf(buf, "%D", np->t_INT);
	else if ((f = np->t_FLOAT) >= -INTLIM && f <= INTLIM
	    && f == (FLOAT)(INT)f)
		sprintf(buf, "%D", (INT)f);
	else if (f > -1e16 && f < 1e16 && modf(f, &ip) == 0.0)
		sprintf(buf, "%.0f", f);	/* whole, past an INT: all its digits */
	else
		sprintf(buf, "%.6g", f);
	return (buf);
}

/*
 * Integer arithmetic result: `i' if the true value `d' fits an INT,
 * else the real `d' -- so 2147483647 + 1 does not wrap.
 */
NODE *
intres(d, i)
FLOAT d;
INT i;
{
	if (d > INTLIM || d < -INTLIM)
		return (fnode(d));
	return (inode(i));
}

/*
 * Does the string look like a number (what field data is judged by)?
 */
static
looksnum(s)
register char *s;
{
	register int d = 0;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '+' || *s == '-')
		s++;
	for ( ; *s >= '0' && *s <= '9'; s++)
		d++;
	if (*s == '.')
		for (s++; *s >= '0' && *s <= '9'; s++)
			d++;
	if (d == 0)
		return (0);
	if (*s == 'e' || *s == 'E') {
		s++;
		if (*s == '+' || *s == '-')
			s++;
		if (*s < '0' || *s > '9')
			return (0);
		while (*s >= '0' && *s <= '9')
			s++;
	}
	while (*s == ' ' || *s == '\t')
		s++;
	return (*s == '\0');
}

/*
 * The truth of an expression, for if/while/for, !, && and || and a
 * bare pattern: a number is true when non-zero; a string when it is
 * not empty -- unless it came from the input (T_STRNUM: a field, $0,
 * a split() piece, or a copy of one) and looks like a number, when it
 * is judged as that number, so a field holding 0 is false while the
 * constant "0" is true.
 */
xtruth(np)
NODE *np;
{
	np = evalexpr(np);
	if (np->t_flag & T_NUM) {
		if (np->t_flag & T_INT)
			return (np->t_INT != 0);
		return (np->t_FLOAT != 0.0);
	}
	if (np->t_STRING[0] == '\0')
		return (0);
	if ((np->t_flag & T_STRNUM) && looksnum(np->t_STRING))
		return (evalfloat(np) != 0.0);
	return (1);
}

/*
 * Post increment -- return the old
 * value before the increment of the
 * node.
 */
NODE *
xinca(np)
register NODE *np;
{
	register NODE *rnp;
	register NODE *enp;

	enp = evalexpr(np);
	rnp = isfloat(enp) ? fnode(evalfloat(enp)) : inode(evalint(enp));
	xassign(np, xadd(enp, &xone));
	return (rnp);
}

/*
 * Post decrement -- return the old value
 * but increment the variable.
 */
NODE *
xdeca(np)
register NODE *np;
{
	register NODE *rnp;
	register NODE *enp;

	enp = evalexpr(np);
	rnp = isfloat(enp) ? fnode(evalfloat(enp)) : inode(evalint(enp));
	xassign(np, xsub(enp, &xone));
	return (rnp);
}
