/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The Bourne shell.
 * This shell is dedicated to Ciaran Gerald Aidan O'Donnell.
 * May he live a thousand minutes.  (long enough to fix up YACC).
 * It is also dedicated to Steve Bourne.
 * May he live a thousand seconds.
 */
%{
#include "sh.h"

#define YYERROR	{yyerrflag=1; goto YYerract; }

extern	NODE	*node();
%}

%union {
	NODE	*yu_node;
	char	*yu_strp;
	int	yu_nval;
}

%token _NULL _DSEMI _ANDF _ORF _NAME _IORS _ASGN
%token _CASE _DO _DONE _ELIF _ELSE _ESAC _FI _FOR _IF _IN _THEN
%token _UNTIL _WHILE _OBRAC _CBRAC

%type <yu_node> command_line	command_list	logical_command
%type <yu_node> pipe_command	command		argument_list
%type <yu_node> argument	control		in_name_list
%type <yu_node> name_list	case_list	case_line
%type <yu_node>	pattern_list	do_list		else_part
%type <yu_node> command_sequence	opt_command_sequence
%type <yu_node> sub_shell

%type <yu_strp>	redirect	name	asgn

%type <yu_nval> whuntile

%%

session:
	session command_line
|
;

command_line:
	'\n' {
		sesp->s_node = NULL;
		reset(RCMD);
		NOTREACHED;
	}
|
	command_list '\n' {
		sesp->s_node = $1;
		reset(errflag ? RERR : RCMD);
		NOTREACHED;
	}
|	error '\n' {
		keyflush();
		keyflag = 1;
		reset(RERR);
		NOTREACHED;
	}
;

if:	_IF optnls ;

then:	_THEN optnls ;

elif:	_ELIF optnls ;

else:	_ELSE optnls ;

whuntile:	_WHILE optnls {	$$ = NWHILE;	}
|	_UNTIL optnls {	$$ = NUNTIL;	}
;

do:	_DO optnls | _DO ';' optnls ;

oror:	_ORF optnls;

andand:	_ANDF optnls;

or:	'|' optnls;

oparen:	'(' optnls ;

obrack:	_OBRAC optnls ;

cparen:	')' optnls ;

dsemi:	_DSEMI optnls ;

command_list:
	logical_command {
		$$ = $1;
	}
|	logical_command '&' {
		$$ = node(NBACK, $1, NULL);
	}
|	logical_command ';' {
		$$ = $1;
	}
|	logical_command '&' command_list {
		$$ = node(NBACK, $1, $3);
	}
|	logical_command ';' command_list {
		$$ = node(NLIST, $1, $3);
	}
;

logical_command:
	pipe_command {
		$$ = $1;
	}
|	pipe_command oror logical_command {
		$$ = node(NORF, $1, $3);
	}
|	pipe_command andand logical_command {
		$$ = node(NANDF, $1, $3);
	}
;

pipe_command:
	command or pipe_command {
		$$ = node(NPIPE, $1, $3);
	}
|	command {
		$$ = $1;
	}
;

command:
	argument_list_init argument_list {
		$$ = node(NCOMS, $2, NULL);
		keypop();
	}
;

argument_list_init:
	{
		keypush();
		keyflag = 1;
	}
;

argument_list:
	argument argument_list {
		if (($1->n_type == NCTRL && $2->n_type == NARGS)
		 || ($1->n_type == NARGS && $2->n_type == NCTRL)) {
			YYERROR;
		}
		($$ = $1)->n_next = $2;
	}
|	argument {
		$$ = $1;
	}
;

argument:
	redirect {
		$$ = node(NIORS, $1, NULL);
	}
|	name {
		$$ = node(NARGS, $1, NULL);
		keyflag = 0;
	}
|	asgn {
		$$ = node(NASSG, $1, NULL);
	}
|	control {
		if ( ! keyflag) {
			YYERROR;
		}
		$$ = node(NCTRL, $1, NULL);
		keyflag = 0;
	}
;

redirect:	_IORS {
		$$ = duplstr(strt, 0);
	}
;

name:	_NAME {
		$$ = duplstr(strt, 0);
	}
;

asgn:	_ASGN {
		$$ = duplstr(strt, 0);
	}
;

control:
	_FOR name in_name_list sep do_list _DONE {
		$$ = node(NFOR, $2, node(NFOR2, $3, node(NLIST, $5, NULL)));
		$$->n_next->n_next->n_next = $$->n_next;
	}
|	_CASE name _IN sep case_list _ESAC {
		$$ = node(NCASE, $2, $5);
	}
|	whuntile command_sequence do_list _DONE {
		$$ = node($1, $2, node(NLIST, $3, NULL));
		$$->n_next->n_next = $$;
	}
|	if command_sequence then opt_command_sequence else_part _FI {
		$$ = node(NIF, node(NNULL, $2, $4), $5);
	}
|	oparen opt_command_sequence ')' {
		$$ = node(NPARN, $2, NULL);
	}
|	obrack opt_command_sequence _CBRAC {
		$$ = node(NBRAC, $2, NULL);
	}
;

in_name_list:
	_IN name_list {
		$$ = $2;
	}
|	{
		$$ = node(NARGS, "\"$@\"", NULL);
	}
;

name_list:
	name name_list {
		$$ = node(NARGS, $1, $2);
	}
|	{
		$$ = NULL;
	}
;

case_list:
	case_line dsemi case_list {
		register NODE *np;

		for (np=$1; np->n_next; np=np->n_next);
		np->n_next = $3;
		$$ = $1;
	}
|	case_line {
		$$ = $1;
	}
|	{
		$$ = NULL;
	}
;

case_line:
	pattern_list cparen opt_command_sequence {
		$$ = node(NCASE2, $3, $1);
	}
;

pattern_list:
	name '|' pattern_list {
		$$ = node(NCASE3, $1, $3);
	}
|	name {
		$$ = node(NCASE3, $1, NULL);
	}
;

do_list:
	do opt_command_sequence {
		$$ = $2;
	}
|	{
		$$ = NULL;
	}
;

else_part:
	elif command_sequence then opt_command_sequence else_part {
		$$ = node(NIF, node(NULL, $2, $4), $5);
	}
|	else opt_command_sequence {
		$$ = node(NELSE, $2, NULL);
	}
|	{
		$$ = NULL;
	}
;

opt_command_sequence:
	command_sequence {
		$$ = $1;
	}
|
	{
		$$ = NULL;
	}
;

command_sequence:
	command_list nls command_sequence {
		$$ = node(NLIST, $1, $3);
	}
|	command_list optnls {
		$$ = $1;
	}
;

sep:	nls
|	';'
|	';' nls
;

optnls:	nls
|
;

nls:	'\n'
|	nls '\n'
;

%%
/*
 * Create a node.
 */
NODE *
node(type, auxp, next)
NODE *auxp, *next;
{
	register NODE *np;

	np = (NODE *) balloc(sizeof (NODE));
	np->n_type = type;
	np->n_auxp = auxp;
	np->n_next = next;
	return (np);
}

#define NBPC 8
#define NKEY 8
static char keys[NKEY] = { 0 };
static int  keyi = NKEY * NBPC;

keyflush()
{
	register char *kp;

	for (kp = keys+NKEY; kp > keys; *--kp = 0);
	keyi = NKEY * NBPC;
}

keypop()
{
	register char	*kp;
	register int	km;

	if ((km = keyi++) >= NKEY * NBPC) {
		panic();
		NOTREACHED;
	}
	kp = keys + (km / NBPC);
	km = 1 << (km %= NBPC);
	keyflag = (*kp & km) ? 1 : 0;
	*kp &= ~km;
}

keypush()
{
	register char	*kp;
	register int	km;

	if ((km = --keyi) < 0) {
		panic();
		NOTREACHED;
	}
	if (keyflag) {
		kp = keys + (km / NBPC);
		km = 1 << (km %= NBPC);
		*kp |= km;
	}
}
/*
 * The following fragments might implement named pipes.
 * The token declaration goes in the header.
 * The nopen production should go with the others of its ilk.
 * The production fragment goes into argument:
%token _NOPEN _NCLOSE
nopen:	_NOPEN optnls ;

|	nopen pipe_command ')' {
		$$ = node(NRPIPE, $2, NULL);
	}
|	oparen pipe_command _NCLOSE {
		$$ = node(NWPIPE, $2, NULL);
	}
 *
 */
