/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Expression reader.
 */
#include <stdio.h>
#include <ctype.h>
#include "roff.h"
#include "code.h"
#include "div.h"
#include "env.h"

/*
 * Given a string containing an expression, a default unit, `mul'/`div',
 * which is used as a multiplier whenever a number is found without a
 * unit attached, an initial value, `num', used if the expression has a
 * leading sign, a flag, `hvf', specifying whether the number is associated
 * with the horizontal or the vertical which is used if the expression has
 * an initial '|' and a default value `def' which is returned scaled by
 * the default unit if no expression is specified.  The expression is
 * evaluated from left to right with no priorites excepting parentheses.
 */
number(str, mul, div, num, hvf, def)
char *str;
long mul, div;
{
	register int n, c;

	expp = str;
	expmul = mul;
	expdiv = div;
	experr = 0;
	while (isascii(*expp) && isspace(*expp))
		expp++;
	if ((c=*expp) == '\0')
		return (unit(def*mul, div));
	if (index("+-|", *expp))
		++expp;
	n = expseq();
	if (*expp != '\0')
		experr++;
	if (experr) {
		printe("Syntax error");
		return (0);
	}
	switch (c) {
	case '+':
		n = num + n;
		break;
	case '-':
		n = num - n;
		break;
	case '|':
		n -= hvf ? cdivp->d_rpos : nlinsiz;
		break;
	}
	return (n);
}

/*
 * Compute an expression sequence.
 */
expseq()
{
	register int n1, n2, c;

	n1 = expval();
	if (experr)
		return (0);
	for (;;) {
		while ((c=*expp++)==' ' || c=='\t')
			;
		switch (c) {
		case '<':
			if (*expp == '=') {
				expp++;
				c = 'l';
			}
			break;
		case '>':
			if (*expp == '=') {
				expp++;
				c = 'g';
			}
			break;
		case '=':
			if (*expp == '=')
				expp++;
			break;
		default:
			if ((c != 0) && index("+-/*%&:", c))
				break;
			--expp;
			return (n1);
		}
		n2 = expval();
		if (experr)
			return (0);
		switch (c) {
		case '+':
			n1 += n2;
			break;
		case '-':
			n1 -= n2;
			break;
		case '*':
			n1 *= n2;
			break;
		case '/':
			if (n2 == 0) {
				printe("Attempted zero divide");
				experr++;
				return (0);
			}
			n1 /= n2;
			break;
		case '%':
			if (n2 == 0) {
				printe("Attempted zero modulus");
				experr++;
				return (0);
			}
			n1 %= n2;
			break;
		case '<':
			n1 = n1 < n2;
			break;
		case '>':
			n1 = n1 > n2;
			break;
		case 'l':
			n1 = n1 <= n2;
			break;
		case 'g':
			n1 = n1 >= n2;
			break;
		case '=':
			n1 = n1 == n2;
			break;
		case '!':
			n1 = n1 != n2;
			break;
		case '&':
			n1 = n1 && n2;
			break;
		case ':':
			n1 = n1 || n2;
			break;
		}
	}
}

/*
 * Get an operand.
 */
expval()
{
	long mul, div, m, d;
	register int n, c;

	while (isascii(c=*expp++) && isspace(c))
		;
	if (c == '(') {
		n = expseq();
		if (*expp++ != ')') {
			--expp;
			experr++;
			n = 0;
		}
		return (n);
	}
	m = 0;
	d = 1;
	while (isascii(c) && isdigit(c)) {
		m = m*10 + c-'0';
		c = *expp++;
	}
	if (c == '.') {
		while (isascii(c=*expp++) && isdigit(c)) {
			m = m*10 + c-'0';
			d *= 10;
		}
	}
	switch (c) {
	case 'i':
		mul = SMINCH;
		div = SDINCH;
		break;
	case 'c':
		mul = SMCENT;
		div = SDCENT;
		break;
	case 'P':
		mul = SMPICA;
		div = SDPICA;
		break;
	case 'm':
		mul = SMEMSP;
		div = SDEMSP;
		break;
	case 'n':
		mul = SMENSP;
		div = SDENSP;
		break;
	case 'p':
		mul = SMPOIN;
		div = SDPOIN;
		break;
	case 'u':
		mul = SMUNIT;
		div = SDUNIT;
		break;
	case 'v':
		mul = SMVLSP;
		div = SDVLSP;
		break;
	default:
		--expp;
		mul = expmul;
		div = expdiv;
	}
	while (isascii(c=*expp) && isalpha(c))
		expp++;
	n = unit(m*mul, d*div);
	return (n);
}

/*
 * Given a long numerator and denominator, divide the numerator by
 * the denominator and return an int.
 */
unit(mul, div)
long mul, div;
{
	if (div == 1)
		return ((int) mul);
	return ((int) (mul/div));
}
