/*
 * make -- maintain program groups
 * td 80.09.17
 * things to do:
 *	upon occasion things aren't freed up that should be
 *	figure out how .DEFAULT works
 *	write a routine to add a string to a token, and use it
 * things done:
 *	20-Oct-82	Made nextc properly translate "\\\n[ 	]*" to ' '.
 *	15-Jan-85	Made portable enough for z-8000, readable enough for
 *			human beings.
 */

#include	"make.h"

/* read characters from backup (where macros have been expanded) or from
 * whatever the open file of the moment is. keep track of line #s.
 */

readc()
{
	if(nbackup!=0)
		return(backup[--nbackup]);
	if(lastc=='\n')
		lineno++;
	lastc=getc(fd);
	return(lastc);
}

/* put c into backup[] */

putback(c)
{
	if(c==EOF)
		return;
	if (nbackup == NBACKUP)
		err("Macro definition too long");
	backup[nbackup++]=c;
}

/* put s into backup */

unreads(s)
register char *s;
{
	register char *t;

	t = &s[strlen(s) - 1];
	while (t >= s)
		putback(*t--);
}

/* return a pointer to the macro definition assigned to macro name s.
 * return NULL if the macro has not been defined.
 */

char *
getdef(s)
register char *s;
{
	register struct macro *i;

	for (i = macro; i != nmacro; ++i)
		if (Streq(s, i->name))
			return (i->value);
	return (NULL);
}

/* install macro with name name and value value in macro[]. Overwrite an
 * existing value if it is not protected.
 */

define(name, value, protected)
register char *name, *value;
{
	register struct macro *i;

	if(dflag)
		printf("define %s = %s\n", name, value);
	for (i = macro; i != nmacro; ++i)
		if (Streq(name, i->name)) {
			if (!i->protected) {
				free(i->value);
				i->value = value;
				i->protected = protected;
			} else if (dflag)
				printf("... definition suppressed\n");
			return;
		}
	if (nmacro == LMACRO)
		err("Too many macro definitions");
	i->name = name;
	i->value = value;
	i->protected = protected;
	++nmacro;
}

/* return the next character from the input file. eat comments, return EOS
 * for a newline not followed by an action, \n for newlines that are followed
 * by actions, $ for $$, put macro expansions in backup[] (no complaint if
 * macro is not defined).
 */

nextc()
{
	register char *s;
	register c;

Again:
	if((c=readc())=='#'){
		do
			c=readc();
		while(c!='\n' && c!=EOF);
	}
	if(c=='\n'){
		if((c=readc())!=' ' && c!='\t'){
			putback(c);
			return(EOS);
		}
		do
			c=readc();
		while(c==' ' || c=='\t');
		putback(c);
		return('\n');
	}
	if(c=='\\'){
		c=readc();
		if(c=='\n') {
			while ((c=readc())==' ' || c=='\t')
				;
			putback(c);
			return(' ');
		}
		putback(c);
		return('\\');
	}
	if(!defining && c=='$'){
		c=readc();
		if(c=='$')
			return(c);
		if(c=='@' || c=='?' || c=='<' || c=='*'){
			putback(c);
			return('$');
		}
		if (c <= ' ' || 0177 <= c)
			err("Bad macro name");
		if(c=='(') {
			s=macroname;
			while(' '<(c=readc()) && c<0177 && c!=')')
				if(s!=&macroname[NMACRONAME])
					*s++=c;
			if (c != ')')
				err("Bad macro name");
			*s++ = '\0';
		} else {
			macroname[0]=c;
			macroname[1]='\0';
		}
		if((s=getdef(macroname))!=NULL)
			unreads(s);
		goto Again;
	}
	return(c);
}

/* cry and die */

die(str)
char	*str;
{
	fprintf(stderr, "make: %r\n", &str);
	exit(ERROR);
}
/* print lineno, cry and die */

err(s)
char *s;
{
	fprintf(stderr, "make: %d: %r\n", lineno, &s);
	exit(ERROR);
}

/* Get a block of l0+l1 bytes copy s0 and s1 into it, and return a pointer to
 * the beginning of the block.
 */

char *
extend(s0, l0, s1, l1)
char *s0, *s1;
{
	register char *t;

 	if (s0 == NULL)
 		t = malloc(l1);
 	else
 		t = realloc(s0, l0 + l1);
 	if (t == NULL)
 		err("Out of space");
	strncpy(t+l0, s1, l1);
	return(t);
}

/* Return 1 if c is EOS, EOF, or one of the characters in s */

delim(c, s)
register char	c;
char	*s;
{
	char	*index();

	return (c == EOS || c == EOF || index(s, c) != NULL);
}

/* Prepare to copy a new token string into the token buffer; if the old value
 * in token wasn't saved, tough matzohs.
 */

starttoken()
{
	token=NULL;
	tokp=tokbuf;
	toklen=0;
}

/* Put c in the token buffer; if the buffer is full, copy its contents into
 * token and start agin at the beginning of the buffer.
 */

addtoken(c)
{
	if(tokp==&tokbuf[NTOKBUF]){
		token=extend(token, toklen-NTOKBUF, tokbuf, NTOKBUF);
		tokp=tokbuf;
	}
	*tokp++=c;
	toklen++;
}

/* mark the end of the token in the buffer and save it in token. */

endtoken()
{
	addtoken('\0');
	token=extend(token, toklen-(tokp-tokbuf), tokbuf, tokp-tokbuf);
}

/* Install value at the end of the token list which begins with next; return
 * a pointer to the beginning of the list, which is the one just installed if
 * next was NULL.
 */

struct token *
listtoken(value, next)
char *value;
struct token *next;
{
	register struct token *p;
	register struct token *t;

	t=(struct token *)malloc(sizeof *t);	/*Necessaire ou le contraire?*/
	if (t == NULL)
		err("Out of space");
	t->value=value;
	t->nexttoken=NULL;
	if(next==NULL)
		return(t);
	for(p=next;p->nexttoken!=NULL;p=p->nexttoken);
	p->nexttoken=t;
	return(next);
}

/* Read macros, dependencies, and actions from the file with name file, or
 * from whatever file is already open. The first string of tokens is saved
 * in a list pointed to by tp; if it was a macro, the definition goes in 
 * token, and we install it in macro[]; if tp points to a string of targets,
 * its depedencies go in a list pointed to by dp, and the action to recreate
 * it in token, and the whole shmear is installed.
 */

input(file)
char *file;
{
	struct token *tp = NULL, *dp = NULL, *np;
	register c;
	char *action;
	int twocolons;

	if(file!=NULL && (fd=fopen(file, "r"))==NULL)
		die("can't open %s", file);
	lineno=1;
	lastc=EOF;
	for(;;){
		c=nextc();
		for(;;){
			while(c==' ' || c=='\t')
				c=nextc();
			if(delim(c, "=:;\n"))
				break;
			starttoken();
			while(!delim(c, " \t\n=:;")){
				addtoken(c);
				c=nextc();
			}
			endtoken();
			tp=listtoken(token, tp);
		}
		switch(c){
		case EOF:
			if(tp!=NULL)
				err("Incomplete line at end of file");
			fclose(fd);
			return;
		case EOS:
			if(tp==NULL)
				break;
		case '\n':
			err("Newline after target or macroname");
		case ';':
			err("; after target or macroname");
/*		SynErr:
 *			err("Syntax error");
 */
		case '=':
			if(tp==NULL || tp->nexttoken!=NULL)
				err("= without macro name or in token list");
			defining++;
			starttoken();
			while((c=nextc())!=EOS && c!=EOF)
				addtoken(c);
			endtoken();
			define(tp->value, token, 0);
			defining=0;
			break;
		case ':':
			if(tp==NULL)
				err(": without preceding target");
			c=nextc();
			if(c==':'){
				twocolons=1;
				c=nextc();
			} else
				twocolons=0;
			for(;;){
				while(c==' ' || c=='\t')
					c=nextc();
				if(delim(c, "=:;\n"))
					break;
				starttoken();
				while(!delim(c, " \t\n=:;")){
					addtoken(c);
					c=nextc();
				}
				endtoken();
				dp=listtoken(token, dp);
			}
			switch(c){
			case ':':
				err("::: or : in or after dependency list");
			case '=':
				err("= in or after dependency");
			case EOF:
				err("Incomplete line at end of file");
			case ';':
			case '\n':
				starttoken();
				while((c=nextc())!=EOS && c!=EOF)
					addtoken(c);
				endtoken();
				action=token;
				break;
			case EOS:
				action=NULL;
			}
			install(tp, dp, action, twocolons);
		}
		while(tp!=NULL){
			np=tp->nexttoken;
			free(tp);
			tp=np;
		}
		while(dp!=NULL){
			np=dp->nexttoken;
			free(dp);
			dp=np;
		}
	}
}


/* Return the last modified date of file with name name. If it's an archive,
 * open it up and read the insertion date of the pertinent member.
 */

time_t
getmdate(name)
char	*name;
{
	char	*subname,
		*lwa;
	int	fd;
	int	magic;
	time_t	result;
	struct ar_hdr	hdrbuf;
	char	*index();

	if (stat(name, &statbuf) ==0)
		return (statbuf.st_mtime);
	subname = index(name, '(');
	if (subname == NULL)
		return (0);
	lwa = &name[strlen(name) - 1];
	if (*lwa != ')')
		return (0);
	*subname = NUL;
	fd = open(name, READ);
	*subname++ = '(';
	if (fd == EOF)
		return (0);
	if (read(fd, &magic, sizeof magic) != sizeof magic || magic != ARMAG) {
		close(fd);
		return (0);
	}
	*lwa = NUL;
	result = 0;
	while (read(fd, &hdrbuf, sizeof hdrbuf) == sizeof hdrbuf) {
		if (strcmp(hdrbuf.ar_name, subname) == 0) {
			result = hdrbuf.ar_date;
			break;
		}
		lseek(fd, hdrbuf.ar_size, REL);
	}
	*lwa = ')';
	return (result);
}


/* Does file name exist? */

exists(name)
char *name;
{
	return(stat(name, &statbuf)>=0);
}

/* Return a pointer to the symbol table entry with name "name", NULL if it's
 * not there.
 */

struct sym *
findsym(name)
char *name;
{
	register struct sym *sp;

	for(sp=sym;sp!=NULL;sp=sp->nextsym)
		if(Streq(name, sp->name))
			return(sp);
	return(NULL);
}

/* Look for symbol with name "name" in the symbol table; install it if it's
 * not there; initialize the action and dependency lists to NULL, the type to
 * unknown, get the modification date, and return a pointer to the entry.
 */

struct sym *
lookup(name)
char *name;
{
	register struct sym *sp;

	if((sp=findsym(name))!=NULL)
		return(sp);
	if ((sp = (struct sym *)malloc(sizeof *sp)) == NULL)	/*necessary?*/
		err("Out of space (lookup)");
	sp->name=name;
	sp->action=NULL;
	sp->deplist=NULL;
	sp->type=T_UNKNOWN;
	sp->moddate=getmdate(sp->name);
	sp->nextsym=sym;
	sym=sp;
	return(sp);
}

/* Install a dependency with symbol having name "name", action "action" in 
 * the end of the dependency list pointed to by nextdep. If s has already 
 * been noted as a file in the dependency list, install action. Return a 
 * pointer to the beginning of the dependency list.
 */

struct dep *
adddep(name, action, nextdep)
char *name, *action;
struct dep *nextdep;
{
	register struct dep *v;
	register struct sym *s;
	struct dep *dp;

	s=lookup(name);
	for(v=nextdep;v!=NULL;v=v->nextdep)
		if(s==v->symbol){
			if (action != NULL) {
				if(v->action!=NULL)
					err("Multiple detailed actions for %s",
						s->name);
				v->action=action;
			}
			return(nextdep);
		}
	if ((v = (struct dep *)malloc(sizeof *v)) == NULL)	/*necessary?*/
		err("Out of core (adddep)");
	v->symbol=s;
	v->action=action;
	v->nextdep=NULL;
	if(nextdep==NULL)
		return(v);
	for(dp=nextdep;dp->nextdep!=NULL;dp=dp->nextdep);
	dp->nextdep=v;
	return(nextdep);
}

/* Do everything for a dependency with left-hand side cons, r.h.s. ante, 
 * action "action", and one or two colons. If cons is the first target in the
 * file, it becomes the default target. Mark each target in cons as detailed
 * if twocolons, undetailed if not, and install action in the symbol table 
 * action slot for cons in the latter case. Call adddep() to actually create
 * the dependency list.
 */

install(cons, ante, action, twocolons)
struct token *ante, *cons;
char *action;
{
	struct sym *cp;
	struct token *ap;

	if(deftarget==NULL && cons->value[0]!='.')
		deftarget=cons->value;
	if(dflag){
		printf("Ante:");
		ap=ante;
		while(ap!=NULL){
			printf(" %s", ap->value);
			ap=ap->nexttoken;
		}
		printf("\nCons:");
		ap=cons;
		while(ap!=NULL){
			printf(" %s", ap->value);
			ap=ap->nexttoken;
		}
		printf("\n");
		if(action!=NULL)
			printf("Action: `%s'\n", action);
		if(twocolons)
			printf("two colons\n");
	}
	for (; cons != NULL; cons = cons->nexttoken) {
		cp=lookup(cons->value);
		if(cp==suffixes && ante==NULL)
			cp->deplist=NULL;
		else{
			if(twocolons){
				if(cp->type==T_UNKNOWN)
					cp->type=T_DETAIL;
				else if(cp->type!=T_DETAIL)
					err("`::' not allowed for %s",
						cp->name);
			} else {
				if(cp->type==T_UNKNOWN)
					cp->type=T_NODETAIL;
				else if(cp->type!=T_NODETAIL)
					err("Must use `::' for %s", cp->name);
				if (action != NULL) {
					if(cp->action != NULL)
						err("Multiple actions for %s",
							cp->name);
					cp->action = action;
				}
			}
			for(ap=ante;ap!=NULL;ap=ap->nexttoken)
				cp->deplist=adddep(ap->value,
					twocolons?action:NULL, cp->deplist);
		}
	}
}

/* If target exists, make it */

maketarget(target)
char *target;
{
	register struct sym *s;

	s=findsym(target);
	if(s==NULL)
		die("Target %s is not defined", target);
	make(s);
}

/* Make s; first, make everything s depends on; if the target has detailed
 * actions, execute any implicit actions associated with it, then execute
 * the actions associated with the dependencies which are newer than s. 
 * Otherwise, put the dependencies that are newer than s in token ($?), 
 * make s if it doesn't exist, and call docmd.
 */

make(s)
register struct sym *s;
{
	register struct dep *dep;
	register char *t;
	int update;
	int type;

	if(s->type==T_DONE)
		return;
	if(dflag)
		printf("Making %s\n", s->name);
	type=s->type;
	s->type=T_DONE;
	for(dep=s->deplist;dep!=NULL;dep=dep->nextdep)
		make(dep->symbol);
	if(type==T_DETAIL){
		implicit(s, "", 0);
		for(dep=s->deplist;dep!=NULL;dep=dep->nextdep)
			if(dep->symbol->moddate>=s->moddate)
				docmd(s, dep->action, s->name,
					dep->symbol->name, "", "");
	} else {
		update=0;
		starttoken();
		for(dep=s->deplist;dep!=NULL;dep=dep->nextdep){
			if(dflag)
				printf("%s time=%ld %s time=%ld\n",
				    dep->symbol->name, dep->symbol->moddate,
				    s->name, s->moddate);
			if(dep->symbol->moddate>=s->moddate){
				update++;
				addtoken(' ');
				for(t=dep->symbol->name;*t;t++)
					addtoken(*t);
			}
		}
		endtoken();
		if (!update && !exists(s->name)) {
			update = TRUE;
			if (dflag)
				printf("`%s' made due to non-existence\n",
					s->name);
		}
		if(s->action==NULL)
			implicit(s, token, update);
		else if(update)
			docmd(s, s->action, s->name, token, "", "");
	}
}



/*
 *	Explen returns the length of the string str after the
 *	macro-variables in it are expanded.
 */

int
explen(str)
register char	*str;
{
	int	result;
	register char	*mp;
	char	*index();

	result = strlen(str);
	while ((str = index(str, '$')) != NULL)
		if ((mp = index(macvars, *++str)) != NULL) {
			++str;
			result += mvarlen[mp - macvars] - 2;
		}
	return (result);
}


/*
 *	Expand returns a pointer to a copy of the string str with
 *	the macro-variables in it expanded.
 */

char	*
expand(str)
register char	*str;
{
	register char	*rptr;
	register char	ch;
	char	*mp;
	char	*result;
	char	*index();

	rptr = (char *)malloc(explen(str) + 1);
	if (rptr == NULL)
		die("Out of space");
	result = rptr;
	while ((ch = *str++) != NUL)
		if (ch == '$' && (mp = index(macvars, *str)) != NULL) {
			strcpy(rptr, mvarval[mp - macvars]);
			rptr += mvarlen[mp - macvars];
			++str;
		} else
			*rptr++ = ch;
	*rptr = NUL;
	return (result);
}


/*
 *	Varexp epands the variable macros (@, ?, <, and *) in the
 *	string str.  It first places the values and their lengths in
 *	the parallel arrays mvarval and mvarlen.
 */

char	*
varexp(str, at, ques, less, star)
char	*str, *at, *ques, *less, *star;
{
	register char	**valp = mvarval;
	register int	*lenp = mvarlen;

	*lenp++ = strlen(at);
	*valp++ = at;
	*lenp++ = strlen(ques);
	*valp++ = ques;
	*lenp++ = strlen(less);
	*valp++ = less;
	*lenp = strlen(star);
	*valp = star;
	return (expand(str));
}


/* Mark s as modified; if tflag, touch s, otherwise execute the necessary
 * commands.
 */

docmd(s, cmd, at, ques, less, star)
struct sym *s;
char *cmd, *at, *ques, *less, *star;
{
	char	*xcmd;
	static char	tcmd[] = "touch ";

	if (dflag)
		printf("ex `%s'\n\t$@=`%s'\n\t$?=`%s'\n\t$<=`%s'\n\t$*=`%s'\n",
			cmd, at, ques, less, star);
	if (qflag)
		exit(NOTUTD);
	s->moddate = now;
	if (tflag) {
		xcmd = (char *)malloc(strlen(tcmd) + strlen(s->name) + 1);
		if (xcmd == NULL)
			die("Out of Space");
		strcpy(xcmd, tcmd);
		strcat(xcmd, s->name);
	} else if (cmd == NULL)
		return;
	else
		xcmd = varexp(cmd, at, ques, less, star);
	doit(xcmd);
	free(xcmd);
}


/* look for '-' (ignore errors) and '@' (silent) in cmd, then execute it
 * and note the return status.
 */

doit(cmd)
register char	*cmd;
{
	register char	*mark;
	int	sflg, iflg;
	int	rstat;
	char	*index();

	if (nflag) {
		printf("%s\n", cmd);
		return;
	}
	do {
		mark = index(cmd, '\n');
		if (mark != NULL)
			*mark = NUL;
		if (*cmd == '-') {
			++cmd;
			iflg = TRUE;
		} else
			iflg = iflag;
		if (*cmd == '@') {
			++cmd;
			sflg = TRUE;
		} else
			sflg = sflag;
		if (!sflg)
			printf("%s\n", cmd);
		fflush(stdout);
		rstat = system(cmd);
		if (rstat != 0 && !iflg)
			if (sflg)
				die("%s	exited with status %d",
					cmd, rstat);
			else
				die("	exited with status %d", rstat);
		cmd = mark + 1;
	} while (mark != NULL && *cmd != NUL);
}


/* Find the implicit rule to generate obj and execute it. Put the name of 
 * obj up to '.' in prefix, and look for the rest in the dependency list 
 * of .SUFFIXES. Find the file "prefix.foo" upon which obj depends, where
 * foo appears in the dependency list of suffixes after the suffix of obj.
 * Then make obj according to the rule from makeactions. If we can't find 
 * any rules, use .DEFAULT, provided we're definite.
 */

implicit(obj, ques, definite)
struct sym *obj;
char *ques;
{
	register char *s;
	register struct dep *d;
	char *prefix, *file, *rulename;
	struct sym *rule;
	struct sym *subj;

	if(dflag)
		printf("Implicit %s (%s)\n", obj->name, ques);
	starttoken();
	for(s=obj->name;*s!='.' && *s!='\0';s++)
		addtoken(*s);
	endtoken();
	if(*s=='\0'){
		free(token);
		if (definite)
			defalt(obj, ques);
		return;
	}
	prefix=token;
	for(d=suffixes->deplist;d!=NULL;d=d->nextdep)
		if(Streq(s, d->symbol->name))
			break;
	if(d==NULL){
		free(prefix);
		if (definite)
			defalt(obj, ques);
		return;
	}
	while((d=d->nextdep)!=NULL){
		starttoken();
		for(s=obj->name;*s!='.';s++)
			addtoken(*s);
		for(s=d->symbol->name;*s;s++)
			addtoken(*s);
		endtoken();
		file=token;
		if(exists(file)){
			starttoken();
			for(s=d->symbol->name;*s!='\0';s++)
				addtoken(*s);
			for(s=obj->name;*s!='.';s++);
			for(;*s!='\0';s++)
				addtoken(*s);
			endtoken();
			rulename=token;
			if((rule=findsym(rulename))!=NULL){
				make(subj=lookup(file));
				if(definite || subj->moddate>=obj->moddate)
					docmd(obj, rule->action,
						obj->name, ques, file, prefix);
				free(prefix);
				free(rulename);
				return;
			}
			free(rulename);
		}
		free(file);
	}
	free(prefix);
	if (definite)
		defalt(obj, ques);
}


/*
 * Deflt uses the commands associated to `.DEFAULT' to make the object
 * `obj'.
 */

defalt(obj, ques)
struct sym	*obj;
char		*ques;
{
	if (deflt == NULL)
		die("Don't know how to make %s", obj->name);
	docmd(obj, deflt->action, obj->name, ques, "", "");
}


main(argc, argv)
char *argv[];
{
	register char	*s, *value;
	struct token	*fp = NULL;
	struct sym	*sp;
	struct dep	*d;
	struct macro	*mp;
	char	*index();

	time(&now);
	++argv;
	--argc;
	while (argc > 0 && argv[0][0] == '-')
		for (--argc, s = *argv++; *++s != NUL;)
			switch (*s) {
			case 'i': iflag++; break;
			case 's': sflag++; break;
			case 'r': rflag++; break;
			case 'n': nflag++; break;
			case 't': tflag++; break;
			case 'q': qflag++; break;
			case 'p': pflag++; break;
			case 'd': dflag++; break;
			case 'f':
				if (--argc < 0)
					Usage();
				fp=listtoken(*argv++, fp);
				break;
			default:
				Usage();
			}
	while (argc > 0 && (value = index(*argv, '=')) != NULL) {
		s = *argv;
		while (*s != ' ' && *s != '\t' && *s != '=')
			++s;
		*s = '\0';
		define(*argv++, value+1, 1);
		--argc;
	}
	suffixes=lookup(".SUFFIXES");
	if (!rflag)
		input("/usr/lib/makemacros");
	deftarget = NULL;
	if (fp == NULL) {
		if ((fd = fopen("makefile", "r")) != NULL)
			input(NULL);
		else
			input("Makefile");
	} else {
		fd = stdin;
		do {
			input( strcmp(fp->value, "-") == 0 ? NULL : fp->value);
			fp = fp->nexttoken;
		} while (fp != NULL);
	}
	if (!rflag)
		input("/usr/lib/makeactions");
	if (findsym(".IGNORE") != NULL)
		++iflag;
	if (findsym(".SILENT") != NULL)
		++sflag;
	deflt = findsym(".DEFAULT");
	if(pflag){
		if(nmacro != macro) {
			printf("Macros:\n");
			for (mp = macro; mp != nmacro; ++mp)
				printf("%s=%s\n", mp->name, mp->value);
		}
		printf("Rules:\n");
		for(sp=sym;sp!=NULL;sp=sp->nextsym){
			if(sp->type!=T_UNKNOWN){
				printf("%s:", sp->name);
				if(sp->type==T_DETAIL)
					putchar(':');
				for(d=sp->deplist;d!=NULL;d=d->nextdep)
					printf(" %s", d->symbol->name);
				printf("\n");
				if(sp->action)
					printf("\t%s\n", sp->action);
			}
		}
	}
	if(argc > 0){
		do{
			maketarget(*argv++);
		} while (--argc > 0);
	} else
		maketarget(deftarget);
	exit(ALLOK);
}

/* Whine about usage and then quit */

Usage()
{
	fprintf(stderr, "%s\n", usage);
	exit(1);
}
