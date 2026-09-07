/*
 * n0/cc0key.c
 * C compiler.
 * Symbol table initialization.
 */

#ifdef   vax
#include "INC$LIB:cc0.h"
#else
#include "cc0.h"
#endif

/*
 * Initialized tokens.
 * Each of these gets linked in place into the hash table.
 * This is a truly sleazy hack; each cxfoo string ends up at the right
 * place to be the t_id member of the TOK.
 */
static struct { TOK t; char id[sizeof "__FILE__"]; } cxfile = {{NULL,NULL}, "__FILE__"};
#define txfile (cxfile.t)
static struct { TOK t; char id[sizeof "__LINE__"]; } cxuline = {{NULL,NULL}, "__LINE__"};
#define txuline (cxuline.t)
static struct { TOK t; char id[sizeof "__DATE__"]; } cxdate = {{NULL,NULL}, "__DATE__"};
#define txdate (cxdate.t)
static struct { TOK t; char id[sizeof "__TIME__"]; } cxtime = {{NULL,NULL}, "__TIME__"};
#define txtime (cxtime.t)
#if	0
/*
 * ANSI C says __STDC__ should be defined as 0 for a non-ANSI compiler,
 * but too many sources use #ifdef __STDC__ when they should use #if __STDC__.
 * This is therefore conditionalized out here and in n0/expand.c.
 */
static struct { TOK t; char id[sizeof "__STDC__"]; } cxstdc = {{NULL,NULL}, "__STDC__"};
#define txstdc (cxstdc.t)
#endif
static struct { TOK t; char id[sizeof "__BASE_FILE__"]; } cxbasefile = {{NULL,NULL}, "__BASE_FILE__"};
#define txbasefile (cxbasefile.t)
static struct { TOK t; char id[sizeof "defined"]; } cxudefined = {{NULL,NULL}, "defined"};
#define txudefined (cxudefined.t)
static struct { TOK t; char id[sizeof "#define"]; } cxdefine = {{NULL,NULL}, "#define"};
#define txdefine (cxdefine.t)
static struct { TOK t; char id[sizeof "#include"]; } cxinclude = {{NULL,NULL}, "#include"};
#define txinclude (cxinclude.t)
static struct { TOK t; char id[sizeof "#undef"]; } cxundef = {{NULL,NULL}, "#undef"};
#define txundef (cxundef.t)
static struct { TOK t; char id[sizeof "#line"]; } cxline = {{NULL,NULL}, "#line"};
#define txline (cxline.t)
static struct { TOK t; char id[sizeof "#assert"]; } cxassert = {{NULL,NULL}, "#assert"};
#define txassert (cxassert.t)
static struct { TOK t; char id[sizeof "#error"]; } cxerror = {{NULL,NULL}, "#error"};
#define txerror (cxerror.t)
static struct { TOK t; char id[sizeof "#pragma"]; } cxpragma = {{NULL,NULL}, "#pragma"};
#define txpragma (cxpragma.t)
static struct { TOK t; char id[sizeof "#if"]; } cxif = {{NULL,NULL}, "#if"};
#define txif (cxif.t)
static struct { TOK t; char id[sizeof "#ifdef"]; } cxifdef = {{NULL,NULL}, "#ifdef"};
#define txifdef (cxifdef.t)
static struct { TOK t; char id[sizeof "#ifndef"]; } cxifndef = {{NULL,NULL}, "#ifndef"};
#define txifndef (cxifndef.t)
static struct { TOK t; char id[sizeof "#else"]; } cxelse = {{NULL,NULL}, "#else"};
#define txelse (cxelse.t)
static struct { TOK t; char id[sizeof "#elif"]; } cxelif = {{NULL,NULL}, "#elif"};
#define txelif (cxelif.t)
static struct { TOK t; char id[sizeof "#endif"]; } cxendif = {{NULL,NULL}, "#endif"};
#define txendif (cxendif.t)
static struct { TOK t; char id[sizeof "#ident"]; } cxident = {{NULL,NULL}, "#ident"};
#define txident (cxident.t)
static struct { TOK t; char id[sizeof "int"]; } cint = {{NULL,NULL}, "int"};
#define tint (cint.t)
static struct { TOK t; char id[sizeof "char"]; } cchar = {{NULL,NULL}, "char"};
#define tchar (cchar.t)
static struct { TOK t; char id[sizeof "float"]; } cfloat = {{NULL,NULL}, "float"};
#define tfloat (cfloat.t)
static struct { TOK t; char id[sizeof "double"]; } cdouble = {{NULL,NULL}, "double"};
#define tdouble (cdouble.t)
static struct { TOK t; char id[sizeof "unsigned"]; } cunsigned = {{NULL,NULL}, "unsigned"};
#define tunsigned (cunsigned.t)
static struct { TOK t; char id[sizeof "signed"]; } csigned = {{NULL,NULL}, "signed"};
#define tsigned (csigned.t)
static struct { TOK t; char id[sizeof "register"]; } cregister = {{NULL,NULL}, "register"};
#define tregister (cregister.t)
static struct { TOK t; char id[sizeof "long"]; } clong = {{NULL,NULL}, "long"};
#define tlong (clong.t)
static struct { TOK t; char id[sizeof "struct"]; } cstruct = {{NULL,NULL}, "struct"};
#define tstruct (cstruct.t)
static struct { TOK t; char id[sizeof "union"]; } cunion = {{NULL,NULL}, "union"};
#define tunion (cunion.t)
static struct { TOK t; char id[sizeof "enum"]; } cenum = {{NULL,NULL}, "enum"};
#define tenum (cenum.t)
static struct { TOK t; char id[sizeof "auto"]; } cauto = {{NULL,NULL}, "auto"};
#define tauto (cauto.t)
static struct { TOK t; char id[sizeof "void"]; } cvoid = {{NULL,NULL}, "void"};
#define tvoid (cvoid.t)
static struct { TOK t; char id[sizeof "static"]; } cstatic = {{NULL,NULL}, "static"};
#define tstatic (cstatic.t)
static struct { TOK t; char id[sizeof "extern"]; } cextern = {{NULL,NULL}, "extern"};
#define textern (cextern.t)
static struct { TOK t; char id[sizeof "goto"]; } cgoto = {{NULL,NULL}, "goto"};
#define tgoto (cgoto.t)
static struct { TOK t; char id[sizeof "return"]; } creturn = {{NULL,NULL}, "return"};
#define treturn (creturn.t)
static struct { TOK t; char id[sizeof "if"]; } cif = {{NULL,NULL}, "if"};
#define tif (cif.t)
static struct { TOK t; char id[sizeof "while"]; } cwhile = {{NULL,NULL}, "while"};
#define twhile (cwhile.t)
static struct { TOK t; char id[sizeof "for"]; } cfor = {{NULL,NULL}, "for"};
#define tfor (cfor.t)
static struct { TOK t; char id[sizeof "do"]; } cdo = {{NULL,NULL}, "do"};
#define tdo (cdo.t)
static struct { TOK t; char id[sizeof "else"]; } celse = {{NULL,NULL}, "else"};
#define telse (celse.t)
static struct { TOK t; char id[sizeof "switch"]; } cswitch = {{NULL,NULL}, "switch"};
#define tswitch (cswitch.t)
static struct { TOK t; char id[sizeof "case"]; } ccase = {{NULL,NULL}, "case"};
#define tcase (ccase.t)
static struct { TOK t; char id[sizeof "default"]; } cdefault = {{NULL,NULL}, "default"};
#define tdefault (cdefault.t)
static struct { TOK t; char id[sizeof "break"]; } cbreak = {{NULL,NULL}, "break"};
#define tbreak (cbreak.t)
static struct { TOK t; char id[sizeof "continue"]; } ccontinue = {{NULL,NULL}, "continue"};
#define tcontinue (ccontinue.t)
static struct { TOK t; char id[sizeof "typedef"]; } ctypedef = {{NULL,NULL}, "typedef"};
#define ttypedef (ctypedef.t)
static struct { TOK t; char id[sizeof "sizeof"]; } csizeof = {{NULL,NULL}, "sizeof"};
#define tsizeof (csizeof.t)
static struct { TOK t; char id[sizeof "short"]; } cshort = {{NULL,NULL}, "short"};
#define tshort (cshort.t)
static struct { TOK t; char id[sizeof "const"]; } cconst = {{NULL,NULL}, "const"};
#define tconst (cconst.t)
static struct { TOK t; char id[sizeof "volatile"]; } cvolatile = {{NULL,NULL}, "volatile"};
#define tvolatile (cvolatile.t)
#ifdef READONLY
static struct { TOK t; char id[sizeof "readonly"]; } creadonly = {{NULL,NULL}, "readonly"};
#define treadonly (creadonly.t)
#endif
#ifdef ALIEN
static struct { TOK t; char id[sizeof "alien"]; } calien = {{NULL,NULL}, "alien"};
#define talien (calien.t)
#endif

/*
 * Initialized symbols.
 * Each of these gets linked in place into the symbol table,
 * and the token each symbol specifies is entered into the token hash.
 * This must be correspond to the symbol order in ktok[] below.
 */
static KEYSYM ktab[] = {
	NULL,	SL_CPP,		XUFILE,
	NULL,	SL_CPP,		XULINE,
	NULL,	SL_CPP,		XUDATE,
	NULL,	SL_CPP,		XUTIME,
#if	0
	NULL,	SL_CPP,		XUSTDC,
#endif
	NULL,	SL_CPP,		XUBASE,
	NULL,	SL_CPP,		XDEFINED,
	NULL,	SL_CPP,		XDEFINE,
	NULL,	SL_CPP,		XINCLUDE,
	NULL,	SL_CPP,		XUNDEF,
	NULL,	SL_CPP,		XLINE,
	NULL,	SL_CPP,		XASSERT,
	NULL,	SL_CPP,		XERROR,
	NULL,	SL_CPP,		XPRAGMA,
	NULL,	SL_CPP,		XIF,
	NULL,	SL_CPP,		XIFDEF,
	NULL,	SL_CPP,		XIFNDEF,
	NULL,	SL_CPP,		XELSE,
	NULL,	SL_CPP,		XELIF,
	NULL,	SL_CPP,		XENDIF,
	NULL,	SL_CPP,		XIDENT,
	NULL,	SL_KEY,		INT,
	NULL,	SL_KEY,		CHAR,
	NULL,	SL_KEY,		FLOAT,
	NULL,	SL_KEY,		DOUBLE,
	NULL,	SL_KEY,		UNSIGNED,
	NULL,	SL_KEY,		SIGNED,
	NULL,	SL_KEY,		REGISTER,
	NULL,	SL_KEY,		LONG,
	NULL,	SL_KEY,		STRUCT,
	NULL,	SL_KEY,		UNION,
	NULL,	SL_KEY,		ENUM,
	NULL,	SL_KEY,		AUTO,
	NULL,	SL_KEY,		VOID,
	NULL,	SL_KEY,		STATIC,
	NULL,	SL_KEY,		EXTERN,
	NULL,	SL_KEY,		GOTO,
	NULL,	SL_KEY,		RETURN,
	NULL,	SL_KEY,		IF,
	NULL,	SL_KEY,		WHILE,
	NULL,	SL_KEY,		FOR,
	NULL,	SL_KEY,		DO,
	NULL,	SL_KEY,		ELSE,
	NULL,	SL_KEY,		SWITCH,
	NULL,	SL_KEY,		CASE,
	NULL,	SL_KEY,		DEFAULT,
	NULL,	SL_KEY,		BREAK,
	NULL,	SL_KEY,		CONTINUE,
	NULL,	SL_KEY,		TYPEDEF,
	NULL,	SL_KEY,		SIZEOF,
	NULL,	SL_KEY,		SHORT,
	NULL,	SL_KEY,		CONST,
	NULL,	SL_KEY,		VOLATILE,
#ifdef READONLY
	NULL,	SL_KEY,		READONLY,
#endif
#ifdef ALIEN
	NULL,	SL_KEY,		ALIEN
#endif
};
#define	NKEYS	sizeof(ktab)/sizeof(KEYSYM)

/*
 * Keyword token pointers, used to initialize symbol table.
 * This must be correspond to the symbol order in ktab[] above.
 */
static TOK *ktok[] = {
	&txfile,
	&txuline,
	&txdate,
	&txtime,
#if	0
	&txstdc,
#endif
	&txbasefile,
	&txudefined,
	&txdefine,
	&txinclude,
	&txundef,
	&txline,
	&txassert,
	&txerror,
	&txpragma,
	&txif,
	&txifdef,
	&txifndef,
	&txelse,
	&txelif,
	&txendif,
	&txident,
	&tint,
	&tchar,
	&tfloat,
	&tdouble,
	&tunsigned,
	&tsigned,
	&tregister,
	&tlong,
	&tstruct,
	&tunion,
	&tenum,
	&tauto,
	&tvoid,
	&tstatic,
	&textern,
	&tgoto,
	&treturn,
	&tif,
	&twhile,
	&tfor,
	&tdo,
	&telse,
	&tswitch,
	&tcase,
	&tdefault,
	&tbreak,
	&tcontinue,
	&ttypedef,
	&tsizeof,
	&tshort,
	&tconst,
	&tvolatile,
#ifdef READONLY
	&treadonly,
#endif
#ifdef ALIEN
	&talien
#endif
};

/*
 * Enter reserved words into the hash table.
 */
kinit()
{
	register char	*p;
	register int	c;
	register TOK	**tpp, *tp;
	register KEYSYM *kp;

	for (kp = ktab, tpp = ktok; kp < &ktab[NKEYS]; ++kp) {
		tp = *tpp++;
#ifdef VALIEN
		if (kp->s_value==ALIEN && notvariant(VALIEN))
			continue;
#endif
#ifdef VREADONLY
		if (kp->s_value==READONLY && notvariant(VREADONLY))
			continue;
#endif
		p = tp->t_id;
		idhash = 0;
		while (c = *p++)
			idhash += c;
		idhash %= NHASH;
		tp->t_tp = hash0[idhash];
		hash0[idhash] = tp;
		tp->t_sym = kp;
	}
}

/* end of n0/cc0key.c */
