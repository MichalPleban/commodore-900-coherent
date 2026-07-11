/*
 * Non-floating ASCII to long conversion
 *
 * long atol(cp)
 * char *cp;
 */

long
atol(cp)
register char *cp;
{
	register long val;
	register c;
	register sign;

	val = sign = 0;
	while ((c = *cp)==' ' || c=='\t')
		cp++;
	if (c == '-') {
		sign = 1;
		cp++;
	} else if (c == '+')
		cp++;
	while ((c = *cp++)>='0' && c<='9')
		val = val*10 - c + '0';
	if (!sign)
		val = -val;
	return (val);
}
