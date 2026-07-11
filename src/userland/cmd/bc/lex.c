#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#include "bc.h"
#include "yy.h"


/*
 *	Add declarations which yacc should really put in bc.lex.h.
 */

extern YYSTYPE yylval;


/*
 *	The jump buffer lexenv is used to hold the environment in
 *	yylex that will be returned to if there is some lexical
 *	error.
 */

static jmp_buf	lexenv;


/*
 *	Yylex is the lexical analyzer for bc.
 */

yylex()
{
	register int	ch;
	register int	nexteq;
	register FILE	*inf = infile;
	static int	newline	= TRUE;		/* TRUE iff at start of line */
	char		*getword(),
			*getqs();

	if (setjmp(lexenv))
		return (ERROR);
Again:
	ch = getc(inf);
	if (newline && ch == '!') {
		shell();
		goto Again;
	}
	while (ch == '\t' || ch == ' ')
		ch = getc(inf);
	if (ch == '\n' || ch == EOF) {
		newline = TRUE;
		return (ch);
	} else
		newline = FALSE;
	if (!isascii(ch))
		lexerr("Illegal character");
	if (islower(ch))
		return (token(getword(ch)));
	if (isdigit(ch) || 'A' <= ch && ch <= 'F' || ch == '.') {
		if (ch == '.') {
			ch = getc(inf);
			ungetc(ch, inf);
			if (! (isascii(ch) && isdigit(ch) ||
				'A' <= ch && ch <= 'F'))
				return (DOT);
			ch = '.';
		}
		yylval.lvalue = getnum(ch);
		return (NUMBER);
	}
	nexteq = next('=');
	switch (ch) {
	case '+':
		return (nexteq ? ADDAB :  (next('+') ? INCR : ch));
	case '-':
		return (nexteq ? SUBAB :  (next('-') ? DECR : ch));
	case '*':
		return (nexteq ? MULAB : ch);
	case '%':
		return (nexteq ? REMAB : ch);
	case '^':
		return (nexteq ? EXPAB : ch);
	case '/':
		if (!nexteq && next('*')) {
			do {
				ch = getc(inf);
			} while (ch != '*' || !next('/'));
			goto Again;
		}
		return (nexteq ? DIVAB : ch);
	case '=':
		return (nexteq ? EQP : ch);
	case '<':
		return (nexteq ? LEP : LTP);
	case '>':
		return (nexteq ? GEP : GTP);
	case '!':
		return (nexteq ? NEP : ch);
	case '"':
		yylval.svalue = getqs();
		return(STRING);
	default:
		if (nexteq)
			ungetc('=', inf);
		return(ch);
	}
}


/*
 *	Next returns TRUE iff the next character on input is "testc".
 *	If the next character is not "testc" then it is pushed back
 *	for later consumption.
 */

int
next(testc)
char testc;
{
	register int ch;
	register int result;

	ch = getc(infile);
	if (! (result = ch == testc))
		ungetc(ch, infile);
	return(result);
}


/*
 *	Getqs reads in a quoted string.  It assumes that the initial
 *	double quote has already been read in and it returns a pointer
 *	to an allocated area where the string has been copyied.
 */

char	*
getqs()
{
	register char	*ptr;
	register int	ch;
	char	buf[MAXSTRING];

	ptr = buf;
	while ((ch = getc(infile)) != '"') {
		if (ch == EOF || ch == '\n')
			lexerr("Unexpected end of quoted string");
		if (ptr < &buf[MAXSTRING])
			*ptr++ = ch;
	}
	if (ptr >= &buf[MAXSTRING])
		lexerr("Quoted string too long");
	*ptr = '\0';
	ptr = (char *)mpalc(1 + ptr - buf);
	strcpy(ptr, buf);
	return (ptr);
}


/*
 *	Token looks up the string pointed to by "word" in the
 *	list of keywords.  If the word is found, then it returns
 *	the corresponding value.  If not, then it makes sure that
 *	the string is in the string table, sets yylval to a pointer to
 *	the string table entry and returns IDENTIFIER.
 */

int
token(word)
char *word;
{
	static struct keyword {
		char *key;
		int keyval;
	} keywords[] = {
		"auto",   AUTO,    "break", BREAK,  "continue", CONTINUE,
		"define", DEFINE,  "do",    DO,     "else",     ELSE,
		"for",    FOR,     "ibase", IBASE,  "if",       IF,
		"length", LENGTH_, "obase", OBASE,  "quit",     QUIT,
		"return", RETURN_, "scale", SCALE_, "sqrt",     SQRT_,
		"while",  WHILE
	};
	register struct keyword *probe;
	register struct keyword *fwa = keywords;
	register struct keyword *lwa = &keywords[nel(keywords) - 1];
	int	cmp;
	dicent	*lookword();

	while (fwa <= lwa) {
		probe = fwa + (lwa - fwa) / 2;
		cmp = strcmp(word, probe->key);
		if (cmp > 0)
			fwa = probe + 1;
		else if (cmp < 0)
			lwa = probe - 1;
		else
			return (probe->keyval);
	}
	yylval.dvalue = lookword(word);
	return (IDENTIFIER);
}


/*
 *	Lookword looks up the string str in the string table dictionary.
 *	If it is not already there, then it adds it and initializes the
 *	type fields to indicate it as undefined.
 *	It returns the pointer to the dictionary entry for the string.
 */

dicent	*
lookword(str)
char	*str;
{
	register dicent	**father,
			*probe;
	register int	rel;

	father = &dictionary;
	while ( (probe = *father) != NULL) {
		rel = strcmp(probe->word, str);
		if (rel == 0)
			return (probe);
		father = (rel < 0 ? &probe->left : &probe->right);
	}
	probe = (dicent *)mpalc((sizeof *probe) + strlen(str) + 1);
	probe->left = probe->right = NULL;
	probe->globalt = probe->localt = UNDEFINED;
	strcpy(probe->word, str);
	*father = probe;
	return (probe);
}


/*
 *	Getword reads in a word (alphanumeric sequence) which starts
 *	with "ch".  It returns a pointer to the string read in.
 *	Note that the pointer is to a static area and hence
 *	the result must be copyied if it is to be used after another
 *	call to getword.
 */

char *
getword(ch)
register char ch;
{
	static char word[MAXWORD + 1];
	register char *chp = &word[0];

	do {
		if (chp < &word[MAXWORD])
			*chp++ = ch;
		ch = getc(infile);
	} while (isascii(ch) && isalnum(ch));
	ungetc(ch, infile);
	if (chp >= &word[MAXWORD])
		lexerr("Identifier too long");
	*chp = '\0';
	return (word);
}


/*
 *	Shell reads in a line from the standard input and forks a shell
 *	to execute it.
 */

shell()
{
	register char	*ptr;
	register int	ch;
	char		buff[MAXSTRING];

	ptr = buff;
	while ((ch = getc(infile)) != '\n' && ch != EOF)
		if (ptr < &buff[MAXSTRING])
			*ptr++ = ch;
	if (ptr >= &buff[MAXSTRING]) {
		fprintf(stderr, "! line too long\n");
		return;
	}
	*ptr = '\0';
	fflush(infile);
	if (system(buff) == NOSHELL)
		fprintf(stderr, "bc: shell couldn't execute\n");
	printf("!\n");
}


/*
 *	Lexerr issues the appropriate error message on stderr and
 *	then longjumps back to lex which return ERROR.
 */

lexerr(str)
char	*str;
{
	fprintf(stderr, "%r\n", &str);
	longjmp(lexenv, TRUE);
}
