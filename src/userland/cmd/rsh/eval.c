#include "sh.h"

/*
 * Evaluation of parameter substitution, command substitution,
 *  blank interpretation, and file name generation.
 */

char	*arcp;		/* character position in argument */
int	argf = 1;	/* First argument flag */
int	argg = 0;	/* Glob seek, escape quoted glob chars */
int	argq = 0;	/* Quotation flag, no blanks or glob */

/*
 * Evaluate a string.
 */
eval(cp, f)
char *cp;
{
	register int m, c;

	strp = strt;
	arcp = cp;
	argf = 1;
	if (f==EHERE) {
		m = MHERE;
		argq = 2;
	} else if (f==EWORD) {
		m = MNQUO;
		argq = 2;
	} else {
		m = MNQUO;
		argq = 0;
	}
	if (f==EARGS || f==EPATT)
		argg = 1;
	else
		argg = 0;

	while ((c=*arcp++) != '\0') {
		if (!class(c, m)) {
			breakup(c);
			continue;
		}
		switch (c) {
		case '"':	/* m == MNQUO || m == MDQUO */
			m = ((argq^=1)&1) ? MDQUO : MNQUO;
			if (m==MDQUO && strcmp(arcp, "$@\"")!=0)
				argf = 0;
			continue;
		case '\'':	/* m == MNQUO */
			while ((c=*arcp++) != '\'')
				qbreakup(c);
			argf = 0;
			continue;
		case '\\':	/* m == MDQUO || m == MNQUO */
			c = *arcp++;
			if (m != MNQUO && ! class(c, m)) {
				breakup('\\');
				breakup(c);
			} else
				qbreakup(c);
			argf = 0;
			continue;
		case '$':	/* m == MNQUO || m = MDQUO */
			evalvar();
			continue;
		case '`':	/* m == MNQUO || m = MDQUO */
			evalcom();
			continue;
		default:
			breakup(c);
			continue;
		}
	}
	if (f==EARGS)
		addlist();
	else
		*strp++ = '\0';
}

/*
 * Read the name of a shell variable and perform the appropriate
 * substitution.
 * Doesn't check for end of buffer.
 */
evalvar()
{
	VAR *vp;
	int s;
	char *wp;
	register int c;
	register char *cp, *pp;

	cp = strp;
	s = '\0';
	c = *arcp++;
	if (class(c, MSVAR)) {
		specvar(c);
		return;
	} else if (c != '{') {
		while (class(c, MRVAR)) {
			*cp++ = c;
			c = *arcp++;
		}
		--arcp;
	} else {
		while (index("}-=?+", c=*arcp++) == NULL)
			*cp++ = c;
		if (c != '}') {
			s = c;
			*cp++ = '=';
			wp = cp;
			while ((c=*arcp++) != '}')
				*cp++ = c;
		}
	}
	*cp++ = '\0';
	if (class((c = *strp), MDIGI)) {
		if ((c -= '1') >= sargc)
			pp = NULL;
		else
			pp = sargp[c];
	} else if (namevar(strp) == 0) {
		eillvar(strp);
		return;
	} else {
		pp = NULL;
		if ((vp=findvar(strp)) != NULL) {
			pp = convvar(vp);
		}
	}
	switch (s) {
	case '\0':
		if (uflag!=0 && pp==NULL)
			enotdef(strp);
		break;
	case '-':
		if (pp == NULL)
			pp = wp;
		break;
	case '=':
		if (pp == NULL) {
			pp = wp;
			if (class(*strp, MDIGI)) {
				printe("Illegal substitution");
				return;
			}
			setsvar(strp);
		}
		break;
	case '?':
		if (pp != NULL)
			break;
		if (*wp != '\0')
			prints("%s\n", wp);
		else {
			*--wp = '\0';
			enotdef(strp);
		}
		reset(RUABORT);
		NOTREACHED;
	case '+':
		if (pp != NULL)
			pp = wp;
		break;
	}
	if (pp == NULL)
		return;
	while ((c=*pp++) != '\0')
		breakup(c);
}

/*
 * Return the value of the special shell variables.
 * No check for end of buffer.
 */
specvar(n)
register int n;
{
	register char *sp;
	register int flag;

	sp = strp;
	switch (n) {
	case '#':
		n = sargc;
		goto maked;
	case '?':
		n = slret;
		goto maked;
	case '$':
		n = shpid;
		goto maked;
	case '!':
		n = sback;
		goto maked;
	maked:
		sprintf(sp, "%d", n);
		break;
	case '-':
		for (sp = &eflag; sp <= &xflag; sp += 1)
			if (*sp)
				breakup(*sp);
		return;
	case '@':
	case '*':
		flag = (argq == 1 && n == '@') ? 1 : 0;
		for (n=0; n<sargc; n++) {
			if (n) {
				argq ^= flag;
				breakup(' ');
				argq ^= flag;
			}
			sp = sargp[n];
			while (*sp)
				breakup(*sp++);
		}
		return;
	case '0':
		sp = sarg0;
		break;
	default:
		if ((n-='1') >= sargc) {
			if (uflag)
				printe("Unset parameter: %c", n+'1');
			return;
		}
		sp = sargp[n];
		break;
	}
	while (*sp)
		breakup(*sp++);
}

/*
 * Read and evaluate a command found between graves.
 */
evalcom()
{
	int pipev[2], f;
	register FILE *fp;
	register int c;
	register int nnl;
	char *cmdp;

	cmdp = arcp;
	while ((c=*arcp++) != '`');
	if ((f = pipeline(pipev)) == 0) {
		dup2(pipev[1], 1);
		close(pipev[0]);
		close(pipev[1]);
		*--arcp = '\0';
		exit(session(SARGS, cmdp));
		NOTREACHED;
	}
	close(pipev[1]);
	if ((fp=fdopen(pipev[0], "r")) == NULL) {
		close(pipev[0]);
		ecantfdop();
		return;
	}
	argf = 1;
	nnl = 0;
	while ((c=getc(fp)) != EOF) {
		if ( ! recover(IEVAL)) {
#ifdef VERBOSE
			if (xflag) prints("Interrupt in eval\n");
#endif
			errflag++;
			break;
		}
		if (c=='\n')
			++nnl;
		else {
			while (nnl) {
				nnl--;
				breakup('\n');
			}
			breakup(c);
		}
	}
	fclose(fp);
	waitc(f);
}

/*
 * breakup adds characters to the current argument.
 * If no quotation is set, it picks off blanks and globs.
 */
breakup(c)
register int c;
{
	if (argq==0) {
		if (index(vifs, c) != NULL) {
			addlist();
			return;
		}
		if (argg && class(c, MGLOB)) {
			dobreakup(c);
			return;
		}
	}
	qbreakup(c);
}

/*
 * qbreakup adds quoted characters to the current argument.
 * if argg is set, then glob characters are quoted with a
 * \, as well as \ itself.
 */
qbreakup(c)
register int c;
{
	if (argg && (class(c, MGLOB) || c == '\\'))
		dobreakup('\\');
	dobreakup(c);
}

/*
 * dobreakup actually adds characters to the current argument
 * and checks for end of buffer.
 */
dobreakup(c)
register int c;
{
	if (strp >= &strt[STRSIZE])	/* Should do more */
		etoolong();
	else
		*strp++ = c;
	argf = 0;
}

/*
 * Addlist terminates the current argument if it is non-empty.
 * If argg is set, then we glob the argument to expand globs or
 * to simply remove any quotes.
 */
addlist()
{
	if (argf != 0)
		return;
	*strp++ = '\0';
	if (argg)
		glob1(duplstr(strt, 0));
	else {
		nargv = addargl(nargv, duplstr(strt, 0));
		nargc += 1;
	}
	strp = strt;
	argf = 1;
	return;
}

/*
 * Evaluate a here document.
 * Unevaluated document is on u2, put the evaluated document there, too.
 */
evalhere(u2)
{
	register int u1;
	register FILE *f2;
	char buf[128];
	char *tmp;

	tmp = shtmp();
	if ((u1=creat(tmp, 0666))<0) {
		ecantmake(tmp);
		return (-1);
	}
	if ((f2=fdopen(u2, "r"))==NULL) {
		ecantfdop();
		close(u1);
		close(u2);
		return (-1);
	}
	while (fgets(buf, 128, f2) != NULL) {
		eval(buf, EHERE);
		write(u1, strt, strp-1-strt);
	}
	close(u1);
	fclose(f2);
	if ((u2 = open(tmp, 0))<0) {
		ecantopen(tmp);
		u2 = -1;
	}
	unlink(tmp);
	return (u2);
}

