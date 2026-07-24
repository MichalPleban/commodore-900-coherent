/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Floating point output conversion routines for `printf'.
 * These use floating point,
 * but don't grind nearly as much as the old ones did!
 */
/*
#include <math.h>
*/
extern	double	frexp();
#define	L10P	17
#define	LOG10B2	0.33219280948873623e+01

/*
 * Convert a floating point
 * number from binary into `f', `e' or
 * `g' format ASCII. The `fmt' argument is
 * the conversion type. The `w' argument is
 * the precision. The characters get put
 * in the buffer `bp'.
 * A pointer past the last character is returned.
 */
char *
_dtefg(fmt, d, w, bp)
double	d;
char	*bp;
{
	register char	*cp1, *cp2;
	int		dscale,
			minusf;
	char		*_dtoa();
	char		tbuf[40];

	if (w < 0)
		w = 6;
	cp1 = _dtoa(fmt, d, w, &dscale, &minusf, tbuf);
	if (fmt == 'g') {
		int	nd;

		cp2 = cp1;
		while (*cp2)
			++cp2;
		while (cp2!=cp1 && cp2[-1]=='0')
			--cp2;
		nd = cp2-cp1;
		if (dscale < -3 || dscale > nd+5)
			fmt = 'e';
		else if (dscale >= nd)
			w = 0;				/* `d' format */
	}
	cp2 = bp;
	if (minusf != 0)
		*cp2++ = '-';
	if (fmt == 'e') {
		*cp2++ = *cp1++;
		if (d != 0.0)
			--dscale;
		for (*cp2++ = '.';  w > 0;  --w)
			*cp2++ = *cp1 ? *cp1++ : '0';
		*cp2++ = 'e';
		if (dscale >= 0)
			*cp2++ = '+';
		else {
			*cp2++ = '-';
			dscale = -dscale;
		}
		*cp2++ = (dscale/10) + '0';
		*cp2++ = (dscale%10) + '0';
	} else {
		if (dscale <= 0)
			*cp2++ = '0';
		else do
			*cp2++ = *cp1 ? *cp1++ : '0';
		while (--dscale);
		if (w != 0) {
			for (*cp2++ = '.';  w > 0;  --w) {
				if (dscale++ < 0)
					*cp2++ = '0';
				else
					*cp2++ = *cp1 ? *cp1++ : '0';
			}
		}
	}
	return (cp2);
}

/*
 * Convert double to string of ascii digits
 * rounded after precision digits behind the decimal point.
 * The decimal scale is stored indirectly through `dscalep' and the sign
 * (non zero if negative) is saved indirectly through `minusfp'.
 */
char *
_dtoa(fmt, d, w, dscalep, minusfp, buf)
double	d;
int	w;
int	*dscalep;
int	*minusfp;
char	*buf;
{
	register char	*cp;
	register int	digit;
	register int	i;
	register int	dscale;
	int		ndigit;
	double		_pow10();

	if (d >= 0.0) {
		*minusfp = 0;
		if (d == 0.0) {
			*dscalep = 0;
			cp = buf;
			while (w-- != 0)
				*cp++ = '0';
			*cp = '\0';
			return (buf);
		}
	} else {
		*minusfp = 1;
		d = -d;
	}
	dscale = _getexp(&d) / LOG10B2;
	d /= _pow10(dscale);
	if (d >= 1.0)
		++dscale;
	else
		d *= 10.0;
	*dscalep = dscale;
	digit = (int) d;
	cp = buf;
	ndigit = w;
	if (fmt == 'f'
	 || fmt == 'g' && 0 <= dscale && dscale <= 5+w)
		ndigit = w+dscale;
	else
		ndigit = w+1;
	if (ndigit < 0)
		ndigit = 0;
	if (ndigit > L10P)
		ndigit = L10P;
	for (i=0; i<ndigit; ++i) {
		*cp++ = digit + '0';
		d = 10.0 * (d-digit);
		digit = (int) d;
	}
	*cp = '\0';
	if (digit < 5)
		return (buf);
	while (cp != buf) {
		--cp;
		if (++*cp <= '9')
			return (buf);
		*cp = '0';
	}
	*cp = '1';
	cp[ndigit] = '\0';
	++*dscalep;
	return (buf);
}

/*
 * Return the binary exponent of a double.
 */
static
_getexp(dp)
double	*dp;
{
	int	exp;

	frexp(*dp, &exp);
	return (exp);
}

/*
 * Compute 10**exp, as a double.
 */
static	int	i;
static
double
_pow10(exp)
register int	exp;
{
	register double	d;

	static	double	_powtab[] = {
		1e0,	1e1,	1e2,	1e3,	1e4,	1e5,
		1e6,	1e7,	1e8,	1e9,	1e10,	1e11,
		1e12,	1e13,	1e14,	1e15 };

	if( exp < 0) {
		return( 1.0 / _pow10( -exp));
	}
	d = 1.0;
	if( exp >= 256) {
		exp -= 256;
		d *= 1e256;
	}
	if( exp >= 128) {
		exp -= 128;
		d *= 1e128;
	}
	if( exp >= 64) {
		exp -= 64;
		d *= 1e64;
	}
	if( exp >= 32) {
		exp -= 32;
		d *= 1e32;
	}
	if( exp >= 16) {
		exp -= 16;
		d *= 1e16;
	}
	d *= _powtab[exp];
	return( d);
/*
	return( d * _powtab[ exp]);	/ this used to blow up cc
*/
}

/*
 * historical interfaces
 */
char *
ecvt(d, w, decp, signp)
double	d;
int	w,
	*decp,
	*signp;
{
	static	char	buf[L10P+1];

	return (_dtoa('e', d, w<=0 ? 0 : w-1, decp, signp, buf));
}

char *
fcvt(d, w, decp, signp)
double	d;
int	w,
	*decp,
	*signp;
{
	static	char	buf[L10P+1];

	return (_dtoa('f', d, w<=0 ? 0 : w, decp, signp, buf));
}

char *
gcvt(d, w, buf)
double	d;
int	w;
char	*buf;
{
	_dtefg('g', d, w, buf);
	return (buf);
}
