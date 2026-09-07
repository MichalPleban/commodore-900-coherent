/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * n0/split/cppeval.c
 * C preprocessor.
 * #if constant expressions for the standalone preprocessor.
 *
 * The integrated compiler evaluates #if through the whole expression
 * parser (n0/expr.c, n0/fold.c and the tree pool).  A preprocessor built
 * without the parser (CPPONLY) evaluates the language #if actually
 * admits: integer and character constants, the arithmetic, shift,
 * relational, bitwise and logical operators, ?:, parentheses and unary
 * - + ! ~.  Identifiers never arrive here: expand() has already replaced
 * macros, `defined' and, with incpp > 1, every unknown name by its text.
 */
#include "cc0.h"

static long ccond();

static
cerr(m) char *m;
{
	cerror(m);
	while (s != EOF)		/* one error, not one per token */
		lex();
}

/*
 * Primary and unary.
 */
static long
cprim()
{
	register long v;

	switch (s) {
	case ICON:
	case LCON:			/* lex() returns both; lval holds either */
		v = lval;
		lex();
		return v;
	case LPAREN:
		lex();
		v = ccond();
		if (s != RPAREN) {
			cerr("missing ')' in #if");
			return 0;
		}
		lex();
		return v;
	case SUB:
		lex();
		return -cprim();
	case ADD:
		lex();
		return cprim();
	case NOT:
		lex();
		return !cprim();
	case COM:
		lex();
		return ~cprim();
	}
	cerr("constant expression required");
	return 0;
}

static int
cprec(op) int op;
{
	switch (op) {
	case OROR:				return 1;
	case ANDAND:				return 2;
	case OR:				return 3;
	case XOR:				return 4;
	case AND:				return 5;
	case EQ: case NE:			return 6;
	case LT: case GT: case LE: case GE:	return 7;
	case SHL: case SHR:			return 8;
	case ADD: case SUB:			return 9;
	case MUL: case DIV: case REM:		return 10;
	}
	return 0;
}

static long
capply(op, l, r) int op; long l, r;
{
	switch (op) {
	case OROR:	return l != 0 || r != 0;
	case ANDAND:	return l != 0 && r != 0;
	case OR:	return l | r;
	case XOR:	return l ^ r;
	case AND:	return l & r;
	case EQ:	return l == r;
	case NE:	return l != r;
	case LT:	return l < r;
	case GT:	return l > r;
	case LE:	return l <= r;
	case GE:	return l >= r;
	case SHL:	return l << r;
	case SHR:	return l >> r;
	case ADD:	return l + r;
	case SUB:	return l - r;
	case MUL:	return l * r;
	case DIV:
	case REM:
		if (r == 0) {
			cerror("division by zero in #if");
			return 0;
		}
		return op == DIV ? l / r : l % r;
	}
	return 0;
}

/*
 * Binary operators by precedence climbing; `min' is the lowest
 * precedence this level may consume, 1 for the whole expression.
 */
static long
cexp(min) int min;
{
	register long l;
	register int op, p;

	l = cprim();
	for (;;) {
		op = s;
		p = cprec(op);
		if (p == 0 || p < min)
			return l;
		lex();
		l = capply(op, l, cexp(p + 1));
	}
}

/*
 * The conditional operator, lowest of all and right associative.
 */
static long
ccond()
{
	long c, a, b;

	c = cexp(1);
	if (s != QUEST)
		return c;
	lex();
	a = ccond();
	if (s != COLON) {
		cerr("missing ':' in #if");
		return 0;
	}
	lex();
	b = ccond();
	return c ? a : b;
}

/*
 * Read a constant expression in cpp.
 * Returns 1 when the expression is FALSE (zero): the callers in
 * n0/sharp.c want the state of the section that follows, and 1 there
 * marks a false section.
 */
cppexpr()
{
	register int ocstate, v;

	ocstate = cstate;
	cstate = 0;			/* so that comments are ignored */
	++incpp;
	lex();
	v = ccond() == 0;
	if (s != EOF)
		cerr("#if expression syntax");
	--incpp;
	cstate = ocstate;
	return v;
}

/* end of n0/cppeval.c */
