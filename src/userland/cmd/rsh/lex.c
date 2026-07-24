/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * A shell.
 * Lexical analyser.
 */
#include "sh.h"
#include "y.tab.h"

/*
 * Local externals.
 */
int	lastget = '\0';		/* Pushed back character */
int	eolflag = 0;		/* End of line */

/*
 * Keyword table.
 */
typedef	struct	key {
	int	k_lexv;			/* Lexical value */
	char	*k_name;		/* Keyword name */
} KEY;

/*
 * Keyword table.
 */
KEY keytab[] ={
	_CASE,	"case",
	_DO,	"do",
	_DONE,	"done",
	_ELIF,	"elif",
	_ELSE,	"else",
	_ESAC,	"esac",
	_FI,	"fi",
	_FOR,	"for",
	_IF,	"if",
	_IN,	"in",
	_THEN,	"then",
	_UNTIL,	"until",
	_WHILE,	"while",
	_OBRAC,	"{",
	_CBRAC, "}",
	_NULL
};
/*
 * Get the next lexical token.
 */
yylex()
{
	register int c;

again:
	while ((c=getn())==' '  ||  c=='\t') ;
	strp = strt;
	if (class(c, MDIGI)) {
		*strp++ = c;
		c = getn();
		if (c=='>' || c=='<') {
			*strp++ = c;
			return (lexiors(c));
		}
		ungetn(c);
		return (lexname());
	}
	if (!class(c, MNAME)) {
		ungetn(c);
		if ((c = lexname()) == 0)
			goto again;
		else if (c < 0)
			return (c);
		if (keyflag) {
			register KEY *kp;
			for (kp=keytab; kp->k_lexv!=_NULL; kp++) {
				if (strcmp(strt, kp->k_name) == 0)
					return (kp->k_lexv);
			}
		}
		return (c);
	}
	*strp++ = c;
	*strp = '\0';
	switch (c) {
	case ';':
		return (isnext(c, _DSEMI));
	case '>':
		return (lexiors(c));
	case '<':
		return (lexiors(c));
	case '&':
		return (isnext(c, _ANDF));
	case '|':
#ifdef NAMEPIPE
		if ( ! isnext(')', 0))
			return (_NCLOSE);
#endif
		return (isnext(c, _ORF));
#ifdef NAMEPIPE
	case '(':
		return (isnext('|', _NOPEN));
#endif
	default:
		return (c);
	}
}

isnext(c, t1)
register int c;
{
	register int c2;

	if ((c2=getn()) == c) {
		*strp++ = c2;
		*strp = '\0';
		return (t1);
	}
	ungetn(c2);
	return (strp[-1]);
}

/*
 * Scan a single argument.
 *	Return 0 if it's an escaped newline, EOF if EOF is found,
 *	or _NAME or _ASGN if any part of an argument is found.
 */
lexname()
{
	int q, asgn;
	register int c, m;
	register char *cp;

	q = 0;
	asgn = 0;
	m = MNQUO;
	cp = strp;
	for (;;) {
		c = getn();
		if (asgn==0)
			asgn = class(c, MBVAR) ? 1 : -1;
		else if (asgn==1)
			asgn = class(c, MRVAR) ? 1 : (c=='=' ? 2 : -1);
		if (cp >= strt + STRSIZE)
			etoolong();
		else
			*cp++ = c;
		if (!class(c, m))
			continue;
		switch (c) {
		case '"':
			m = (q^=1) ? MDQUO : MNQUO;
			continue;
		case '\'':
			strp = cp;
			if ((c = collect('\'', 1)) != '\'')
				break;
			cp = strp;
			continue;
		case '\\':
			if ((c=getn()) < 0) {
				syntax();
				break;
			}
			if (c == '\n') {
				ungetn((c=getn())<0 ? '\n' : c);
				if (--cp == strp)
					return (0);
				continue;
			}
			*cp++ = c;
			continue;
		case '$':
			if ((c=getn()) == '{') {
				*cp++ = c;
				strp = cp;
				if ((c = collect('}', 0)) != '}')
					break;
				cp = strp;
				continue;
			}
			ungetn(c);
			continue;
		case '`':
			strp = cp;
			if ((c = collect('`', 0)) != '`')
				break;
			cp = strp;
			continue;
		}
		break;
	}
	if (c < 0)
		return (c);
	if (q) {
		emisschar('"');
		*cp = '\0';
	} else {
		*--cp = '\0';
	}
	ungetn(c);
	strp = cp;
#ifdef VERBOSE
	if (vflag)
	prints("\t<%d> <%s> %s\n", getpid(), (asgn==2 ? "ASGN" : "NAME"), strt);
#endif
	if (errflag)
		return (_NULL);
	else if (asgn==2)
		return (_ASGN);
	else
		return (_NAME);
	return (asgn==2 ? _ASGN : _NAME);
}

/*
 * Lex an io redirection string, including the file name if any.
 *	Called with one '>' or '<' in buffer, optionally preceded by
 *	a digit.
 */
lexiors(c1)
{
	register int c;
	register char *name;
	char *tmp, *iors;
	int hfd, quote;
	BUF **bpp;

	*strp++ = c = getn();
	if (c=='&') {
		*strp++ = c = getn();
		*strp = '\0';
		if (c < 0) return (c);
		if (c!='-' && !class(c, MDIGI))
			eredir();
		return (_IORS);
	}
	if (c==c1)
		c1 += 0200;
	else {
		*--strp = '\0';
		ungetn(c);
	}
	/* Collect file name */
	while ((c=getn())==' '||c=='\t')
		*strp++ = c;
	ungetn(c);
	name = strp;
	if (c=='\n') {
		eredir();
		return (_IORS);
	}
	while ((c = lexname())==0);
	if (c < 0) return (c);
	if (c1!='<'+0200)
		return (_IORS);
	/* Collect here document */
	if ((c=getn())!='\n') {
		eredir();
		++strp;
		c = collect('\n', 1);
	}
	if (c < 0) return (c);
	bpp = savebuf();
	strp = strt;
	/* Simplify quoted to ?<file from ?<<file */
	if (quote = (int)any(name, "\"\\'"))
		*++strp = *strt;
	tmp = name;
	name = duplstr(name, 0);
	strcpy(tmp, shtmp());
	iors = duplstr(strp, 0);
	tmp += iors - strp;
	eval(name, EWORD);
	name = duplstr(strcat(strt, "\n"), 0);
	if ((hfd = creat(tmp, 0666)) < 0)
		ecantmake(tmp);
	for (;;) {
		strp = strt;
		if ((c = collect('\n', 2)) < 0)
			break;
		*strp = '\0';
		if (strcmp(strt, name)==0)
			break;
		if (hfd < 0)
			continue;
		if (! quote && strp > strt + 1 && strp[-2]=='\\')
			*(strp-=2) = '\0';
		if (! quote && *strt=='\\' && strcmp(name, strt+1)==0)
			write(hfd, strt+1, strp-strt-1);
		else
			write(hfd, strt, strp-strt);
	}
	close(hfd);
	cleanup(0, tmp);
	ungetn('\n');
	strcpy(strt, iors);
	freebuf(bpp);
	/* Check for interrupt, since EOF is legal for once */
	if (c < 0 && ! recover(ILEX)) return (c);
	return (_IORS);
}

/*
 * Collect characters until the end character is found.  If `f' is
 * set, all characters are passed through otherwise '\' escapes the
 * next character and newline is not allowed.
 * If `f' is set to 2, then no error is desired.
 */
collect(ec, f)
register int ec;
{
	register int c;
	register char *cp;

	cp = strp;
	while ((c=getn()) != ec) {
		if (c<0 || (c=='\n' && f==0)) {
			if (--f <= 0)
				emisschar(ec);
			return (c);
		}
		if (c=='\\' && f==0) {
			if ((c=getn()) < 0) {
				syntax();
				return (c);
			}
			if (c == '\n')
				continue;
		}
		if (cp >= strt + STRSIZE)
			etoolong();
		else
			*cp++ = c;
	}
	*cp++ = ec;
	strp = cp;
	return (ec);
}

/*
 * Get a character.
 */
getn()
{
	register int c;
	register int t;

	if (lastget != '\0') {
		c = lastget;
		lastget = '\0';
		return (c);
	}
	switch (t = sesp->s_type) {
	case SSTR:
	case SFILE:
		yyline += eolflag;
		eolflag = 0;
		if (prpflag && sesp->s_flag) {
			if (prpflag -= 1) {
				prompt("\n");
				prpflag -= 1;
			}
			prompt(comflag ? vps1 : vps2);
			comflag = 0;
		}
		if ((c=getc(sesp->s_ifp))=='\n') {
			if (sesp->s_flag) {
				prpflag = 1;
				yyline = 1;
			} else
				eolflag = 1;
		}
		if (vflag)
			putc(c, stderr);
		return (c);
	case SARGS:
	case SARGV:
		if (sesp->s_flag)
			return (EOF);
		if ((c=*sesp->s_strp++) == '\0') {
			if (t == SARGV
			 && (sesp->s_strp=*++sesp->s_argv) != NULL)
				c = ' ';
			else {
				sesp->s_flag = 1;
				c = '\n';
			}
		}
		if (vflag)
			putc(c, stderr);
		return (c);
	}
}

/*
 * Unget a character.
 */
ungetn(c)
{
	lastget = c;
}


/*
 * Returns true if the intersection of two
 * strings is non-NULL, otherwise 0.
 */
char *
any(s, spcl)
char *s, *spcl;
{
	register char *p1, *p2;

	for (p1 = s; *p1; p1++)
		for (p2 = spcl; *p2; p2++)
			if (*p2 == *p1)
				return (p1);
	return (NULL);
}

