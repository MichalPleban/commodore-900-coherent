/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Do the atof function in C.
 */
double
atof(cp)
register char *cp;
{
	register int c;				/* characters */
	register int exp;			/* decimal exponent */
	int negnum, negexp, gotdot;		/* flags */
	double	d;				/* accumulator */

	d = 0.0;
	exp = 0;
	negnum = negexp = gotdot = 0;

	/*
	 * Scan off white space.
	 */
	c = *cp;
	while (c == ' '  ||  c == '\t')
		c = *++cp;

	/*
	 * Handle a leading plus or minus sign.
	 */
	if (c == '+')
		c = *++cp;
	else if (c == '-') {
		negnum = 1;
		c = *++cp;
	}

	/*
	 * Build the number from digits, up to the exponent (if any).
	 * Look for a decimal point, and count the digits after it.
	 */
	for (;;) {
		if ('0' <= c  &&  c <= '9') {
/*
			printf( " before d *= 10.0\n");
			printf( " hi word d = %x\n", *(int *)&d);
*/
			d *= 10.0;
/*
			printf( " hi word d = %x\n", *(int *)&d);
			printf( " before d += c - '0'\n");
*/
			d += (c - '0');
/*
			printf( " after d += c - '0'\n");
			printf( " hi word d = %x\n", *(int *)&d);
*/
			if (gotdot)
				--exp;
			c = *++cp;
		} else if (c == '.') {
			if (gotdot)
				break;
			gotdot = 1;
			c = *++cp;
		} else
			break;
	}

	/*
	 * If there is an exponent build it in a temporary and
	 * combine it with "exp". Look for a leading '+' or '-'.
	 */
	if (c == 'e'  ||  c == 'E') {
		int exp2 = 0;

		c = *++cp;
		if (c == '+')
			c = *++cp;
		else if (c == '-') {
			negexp = 1;
			c = *++cp;
		}
		while ('0' <= c  &&  c <= '9') {
			exp2 *= 10;
			exp2 += (c - '0');
			c = *++cp;
		}
		if (negexp)
			exp2 = -exp2;
		exp += exp2;
	}

	/*
	 * Reconcile the number with the exponent.
	 */
	if (exp > 0) {
		while (exp--)
			d *= 10.0;
	} else if (exp < 0) {
		double	d1 = 1.0;

		while (exp++ < 0)
			d1 *= 10.0;
		d /= d1;
	}

	/*
	 * That's it!
	 */
	if( negnum)
		d = -d;
/*
	printf( " return hi d = %x\n", *(int *)&d);
*/
	return (d);
}
