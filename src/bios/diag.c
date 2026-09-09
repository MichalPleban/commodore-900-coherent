/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Hard disk diagnostic stubs, plus the debugger's data tables.
 * The tables live here (not with the debugger code) so that link
 * order places them at their proper data addresses.
 */

extern char sigtext[], helptext[], m_err[];

/* display-format state; the debugger keeps its base address at +0x12 */
char dbgfmt[] = {
	0x00,0x02,0x00,0x00,0x20,0x34,0x00,0x00,0x00,0x00,0x20,
	0x34,0x00,0x00,0x00,0x00,0x20,0x34,0x3a,0x00,0x00,0x00,
};

/* trap/signal names, indexed by trap number (reached as dbgfmt+0x16) */
char *signame[] = {
	sigtext,
	sigtext+10,
	sigtext+17,
	sigtext+27,
	sigtext+32,
	sigtext+38,
	sigtext+50,
	sigtext+58,
	sigtext+70,
	sigtext+82,
	sigtext+87,
	sigtext+98,
	sigtext+111,
	sigtext+137,
	sigtext+160,
	sigtext+183,
};

/* word-register name table: token, long-display flag, name */
struct rname {
	char	r_val;
	char	r_lflg;
	char	r_name[4];
};
struct rname regtab[] = {
	{ 0xeb, 0, "r0" },
	{ 0xec, 0, "r1" },
	{ 0xed, 0, "r2" },
	{ 0xee, 0, "r3" },
	{ 0xef, 0, "r4" },
	{ 0xf0, 0, "r5" },
	{ 0xf1, 0, "r6" },
	{ 0xf2, 0, "r7" },
	{ 0xf3, 0, "r8" },
	{ 0xf4, 0, "r9" },
	{ 0xf5, 0, "r10" },
	{ 0xf6, 0, "r11" },
	{ 0xf7, 0, "r12" },
	{ 0xf8, 0, "r13" },
	{ 0xf9, 0, "r14" },
	{ 0xfa, 0, "r15" },
	{ 0xe9, 1, "nsp" },
	{ 0xfd, 0, "fcw" },
	{ 0xfe, 1, "pc" },
};

/* long-register name table */
struct lname {
	char	l_val;
	char	l_name[5];
};
struct lname regtabl[] = {
	{ 0xeb, "rr0" },
	{ 0xed, "rr2" },
	{ 0xef, "rr4" },
	{ 0xf1, "rr6" },
	{ 0xf3, "rr8" },
	{ 0xf5, "rr10" },
	{ 0xf7, "rr12" },
	{ 0xf9, "rr14" },
	{ 0xe9, "nsp" },
	{ 0xfe, "pc" },
};

/* debugger input line buffer */
char ddtline[96] = { 0 };

/* menu help lines, one per command, in command order */
char *cmdtab[] = {
	helptext,
	helptext+40,
	helptext+85,
	helptext+129,
	helptext+170,
	helptext+208,
	helptext+250,
	helptext+293,
	helptext+326,
	helptext+377,
	helptext+425,
	helptext+458,
	helptext+499,
	helptext+537,
	helptext+578,
	helptext+617,
	helptext+654,
	helptext+695,
};

/* debugger scratch words */
char ddtscr[6] = { 0 };

/* error messages, indexed by error code (reached as ddtscr+0x06) */
char *errtab[] = {
	m_err+3,	/* Illegal command */
	m_err+19,	/* Syntax error */
	m_err+32,	/* Odd address */
	m_err+44,	/* Duplicate breakpoint */
};

/*
 * String pools.  Entries are NUL-separated; the closing quote of each
 * pool supplies the final entry's NUL.
 */
char sigtext[] = "\
USER CALL\0\
Hangup\0\
Interrupt\0\
Quit\0\
Alarm\0\
Termination\0\
Restart\0\
System call\0\
Pipe broken\0\
Kill\0\
Breakpoint\0\
Segment trap\0\
Unimplemented instruction\0\
Privileged instruction\0\
Non-vectored interrupt\0\
Non-maskable Interrupt";

/* the '?' menu, one line per command */
char helptext[] = "\
a addr                set base address\n\0\
bc                    clear all breakpoints\n\0\
bd                    show all breakpoints\n\0\
br bpt#               remove breakpoint\n\0\
bs bpt# addr          set breakpoint\n\0\
c                     continue from trap\n\0\
e addr [nword]        edit/display memory\n\0\
f addr nword value    fill area\n\0\
h value1 value2       compute hex sum, difference\n\0\
i port                input (output) from port\n\0\
m addr1 addr2 nword   move data\n\0\
M                     display mmu setup\n\0\
o port                output to port\n\0\
r                     display registers\n\0\
R regname value       modify register\n\0\
s [nword]             display stack\n\0\
S seg base attr len   remap mmu segment\n\0\
?                     display this menu\n";

/*
 * Debugger messages, split on even-byte boundaries.
 * Memory-dump column gap, then the breakpoint report pieces:
 * "\nBreakpoint " n " at " seg:off ": ".
 */
char ddtmsgs[] = "  \0\nUnknown Breakpoint at \0\nBreakpoint \0 at \0: ";
/* menu banner; " = " for the bd listing; trace toggle; the prompt's ": " */
char m_menu[] = "Type '?' for a menu of commands\n\0 = \0\ntrace = \0: ";
/* prompt indent, then the errtab messages and their "\n? " prefix */
char m_err[] = "  \0Illegal command\0Syntax error\0Odd address\0Duplicate breakpoint\0\n? ";

hddiagnostic()
{
}
hdparams()
{
}
