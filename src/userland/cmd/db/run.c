/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - execution control (binary 0x1576..0x2785).
 *
 * The run/step loop (plant breakpoints, ptrace 7/9, wait, dispatch on the
 * stop), child creation with redirection, the command reader loop, the
 * `=' patch command, the `!' shell escape and the `?' display driver.
 *
 * Reconstructed from the shipped binary (_disasm/db.asm); the analysis
 * copy with the original addresses is _disasm/db_part2.c.
 */

#include "db.h"

#define	NBPT	16		/* breakpoint slots at bpts (0x1ba0) */
#define	NEXPR	10		/* expression list length (parsed by exprlist) */
#define	NARG	19		/* argv slots built for the child */
#define	BPTINSTR 0x7f81		/* `sc #0x81' -- the breakpoint instruction */

/*
 * A parsed expression (6 bytes), filled by expr / exprlist.
 * e_flags bit 0: value omitted (default applies); bit 1: a space was named.
 */

/*
 * A breakpoint slot (24 bytes).  b_flags bits: 1 = user breakpoint (:b,
 * commands in b_cmd), 2 = frame-matched breakpoint (commands in b_rcmd,
 * frame in b_frame), 4 = "step over call" return breakpoint (frame in
 * b_frame4, planted by retbpt).
 */

/* Input-stack node (10 bytes) kept in insrc; kind 2 = string, else FILE */

extern int	errno;			/* absolute 0xfffe */


/* 0x1576 runchild() — the execution loop: plant breakpoints, run or step the child, act on the stop */
runchild()
{
	register struct bpt *bp;
	register int n;
	register int found;		/* pc was at a breakpoint when we stopped */
	register int atbpt;		/* a breakpoint is planted at the resume pc */
	register long pc;
	int fault;
	long fp;
	char *s;

	found = 0;
	s = ":x\n";
runcmds:
	docmds(s);
top:
	stepmode = 0;
	atbpt = 0;
	if (runmode != 0 && runmode != 4) {
		stepmode = 1;
		if (runmode == 2)
			chkcall();
	}
	pc = getpc();
	for (n = NBPT, bp = bpts; n-- != 0; bp++) {
		if (bp->b_flags == 0)
			continue;
		dot = bp->b_addr;
		if (rdmem(1, &bp->b_save, 2) == 0) {
			s = "cannot read\n";
			goto fail;
		}
		if (found && bp->b_addr == pc) {
			stepmode = 1;
			continue;
		}
		dot = bp->b_addr;
		if (wrmem(1, &bptinst, 2) == 0) {
			s = "cannot install\n";
			goto fail;
		}
	}
	found = 0;
	if (isbpt(pc))
		atbpt = 1;
	if (stepprep() == 0)
		goto dead;
	errno = 0;
	ptrace(stepmode ? 9 : 7, childpid, 1L, 0);
	if (errno) {
		error("Ptrace error (%d)", errno);
		goto dead;
	}
	if (waitchild() == 0)
		goto dead;
	if (tmpundo() == 0)
		goto dead;
	if (getregs() == 0)
		goto dead;
	if ((fault = getfault()) == 0)
		goto dead;
	for (n = NBPT, bp = bpts; n-- != 0; bp++) {
		if (bp->b_flags == 0)
			continue;
		dot = bp->b_addr;
		if (wrmem(1, &bp->b_save, 2) == 0) {
			s = "cannot replace instr\n";
			goto fail;
		}
	}
	if (fault != 10) {		/* not SIGBPT */
		s = ":f\n:x\n";
		goto runcmds;
	}
	pc = getpc() - 2;
	if (stepmode == 0 || atbpt) {
		for (n = NBPT, bp = bpts; n-- != 0; bp++) {
			if (bp->b_flags && bp->b_addr == pc) {
				setpc(pc);
				found = 1;
				break;
			}
		}
	}
	switch (runmode) {
	case 1:
	case 2:
		docmds(stepcmds);
		if (--stepcnt != 0)
			goto top;
		s = ":x\n";
		goto runcmds;
	case 3:
		for (n = 0, bp = bpts; n < NBPT; n++, bp++)
			bp->b_flags &= ~4;
		runmode = 4;
		retbpt();
		break;
	}
	/* NOTE: bp is never cleared by an exhausted search above, so this
	 * test only sees the pointer left by the last loop; kept as coded. */
	if (bp == NULL) {
		if (stepmode && atbpt == 0)
			goto top;
		s = ":f\n:x\n";
		goto runcmds;
	}
	fp = getfp();
	if (bp->b_flags & 4) {
		if (bp->b_frame4 == 0 || bp->b_frame4 == fp) {
			bp->b_flags &= ~4;
			if (runmode == 4) {
				runmode = 2;
				docmds(stepcmds);
				if (--stepcnt != 0)
					goto top;
				s = ":x\n";
				goto runcmds;
			}
		}
	}
	if (bp->b_flags & 2) {
		if (fp == bp->b_frame) {
			bp->b_flags &= ~2;
			s = bp->b_rcmd;
			goto runcmds;
		}
	}
	if (bp->b_flags & 1) {
		s = bp->b_cmd;
		goto runcmds;
	}
	goto top;
fail:
	fprintf(stderr, s);
	bpterror(bp->b_addr);
dead:
	killchild();
	reloadobj();
	bptclear();
	found = 0;
	s = ":x\n";
	goto runcmds;
}

/* 0x1940 reloadobj(hdr) — re-read and check the object file header, reload the symbol table */
reloadobj()
{
	struct ldheader hdr;		/* NOTE: read as 0x30 bytes (the on-disk header size) */

	freeallmaps();
	objloaded = 0;
	fseek(objfp, 0L, 0);
	if (fread(&hdr, 0x30, 1, objfp) != 1) {
		error("Can't read object file");
		return (0);
	}
	canhdr(&hdr);
	if (hdr.l_magic != L_MAGIC) {
		error("not an object file");
		return (0);
	}
	objloaded = 1;
	maplout(&hdr);
	return (1);
}

/* 0x19f0 execcmd() — `:e': split the rest of the line into argv/redirections and start the child; 1 on error */
execcmd()
{
	register char *p, *q;
	register int c;
	int argc, append, quote;
	char *argv[NARG + 1];
	char *outfile, *infile;
	char *s;

	killchild();
	if (objloaded == 0) {
		s = "No l.out";
		goto err;
	}
	infile = NULL;
	outfile = NULL;
	quote = 0;
	append = 0;
	argc = 0;
	p = q = linebuf;
	c = *p++;
	for (;;) {
		if (c == '\n')
			break;
		if (c == '<') {
			infile = q;
			c = *p++;
		} else if (c == '>') {
			outfile = q;
			c = *p++;
			if (c == '>') {
				append = 1;
				c = *p++;
			}
		} else {
			if (argc >= NARG) {
				s = "Too many arguments";
				goto err;
			}
			argv[argc++] = q;
		}
		for (;;) {
			if (quote == 0 && isascii(c) && isspace(c))
				break;
			if (c == '\n')
				break;
			if (c == '"') {
				quote ^= 1;
				c = *p++;
				continue;
			}
			if (c == '\\') {
				c = *p++;
				if (c == '\n') {
					s = "Syntax error";
					goto err;
				}
			}
			*q++ = c;
			c = *p++;
		}
		if (quote) {
			s = "Missing \"";
			goto err;
		}
		*q++ = '\0';
		if (c == '\n')
			break;
		while (isascii(c) && isspace(c))
			c = *p++;
	}
	if (argc == 0)
		argv[argc++] = objname;
	argv[argc] = NULL;
	if (startchild(argv, infile, outfile, append))
		return (0);
	return (1);
err:
	synerr(s);
	return (1);
}

/* 0x1bc4 startchild(argv, infile, outfile, append) — fork the child, redirect, trace-me, exec; build the maps[0] */
startchild(argv, infile, outfile, append)
char *argv[];
char *infile, *outfile;
int append;
{
	register int fd;

	if ((childpid = fork()) < 0) {
		error("Cannot fork");
		return (0);
	}
	if (childpid == 0) {
		if (infile != NULL) {
			if ((fd = open(infile, 0)) < 0)
				fatal("Cannot open %s", infile);
			dup2(fd, 0);
			close(fd);
		}
		if (outfile != NULL) {
			fd = -1;
			if (append) {
				if ((fd = open(outfile, 1)) >= 0)
					lseek(fd, 0L, 2);
			}
			if (fd < 0) {
				if ((fd = creat(outfile, 0644)) < 0)
					fatal("%s: cannot create", outfile);
			}
			dup2(fd, 1);
			close(fd);
		}
		ptrace(0, 0, 0L, 0);
		execv(objname, argv);
		exit(1);
	}
	if (waitchild() == 0)
		return (0);
	freeallmaps();
	maps[0] = mkmap(0L, 0L, 0x7fffffffL, 0L, ptread, ptwrite, 1);
	maps[1] = mkmap(0L, 0L, 0x7fffffffL, 0L, ptread, ptwrite, 0);
	maps[2] = mkmap(0L, 0L, 0x800L, 0L, ptread, ptwrite, 2);
	childalive = 1;
	regsok = 1;
	return (1);
}

/* 0x1df8 stopstate() — after a stop: back up over the breakpoint, fetch registers, return the fault type */
stopstate()
{
	if (tmpundo() == 0)
		return (0);
	if (getregs() == 0)
		return (0);
	return (getfault());
}

/* 0x1e22 killchild() — kill the child if there is one and forget it */
killchild()
{
	if (childalive) {
		ptrace(8, childpid, 0, 0);
		waitchild();
	}
	childalive = 0;
	regsok = 0;
	faultname = NULL;
}

/* 0x1e6a waitchild() — wait for the child; 1 if it stopped, 0 if it is gone */
waitchild()
{
	register int pid;
	int status;

	for (;;) {
		pid = wait(&status);
		if (pid == childpid)
			break;
		if (pid >= 0) {
			error("Adopted a child %d", pid);
			continue;
		}
		if (intrflag == 0) {
			childalive = 0;
			error("Nonexistant child");
			return (0);
		}
		intrflag = 0;
	}
	if ((status & 0xff) == 0x7f)
		return (1);
	childalive = 0;
	error("Child process terminated (%d)", (status >> 8) & 0xff);
	return (0);
}

/* 0x1f0e docmds(s) — run the commands in string s (after discarding any pending input) */
docmds(s)
char *s;
{
	register struct insrc *ip;

	while ((ip = insrc) != NULL) {
		insrc = ip->i_next;
		release(ip);
	}
	dsize = 2;
	runreq = 0;
	dotaddr = getpc();
	ungotc = 0;
	pushstr(s);
	cmdloop();
	intrseen();
}

/* 0x1f74 cmdloop() — the command reader: expression list, then newline, `:', `=' or `?' */
cmdloop()
{
	register int c;
	register int space;
	register long val;
	register char *p;
	struct opnd e[NEXPR];
	long save;

	for (;;) {
		c = getch();
		if (c == -1)
			return;
		if (c == '!') {
			shellcmd();
			continue;
		}
		if (c == '?') {
			c = getch();
			if (c != '\n') {
				synerr("Syntax error");
				goto skip;
			}
			if (lasterr == NULL)
				print("No error\n");
			else
				print("%s\n", lasterr);
			continue;
		}
		ungetch(c);
		if (exprlist(e) == 0)
			goto skip;
		c = getch();
		switch (c) {
		case '\n':
			if (eempty(e) && runreq) {
				if (childalive == 0) {
					runreq = 0;
					synerr("Cannot single step");
					goto skip;
				}
				runmode = runreq;
				stepcnt = evalue2(&e[1], 1L);
				return;
			}
			/* fall into the display path with the newline pushed back */
		case '=':
			ungetch(c);
		case '?':
			runreq = 0;
			space = espace(e, dotspace);
			c = getch();
			if (c == '=') {
				val = evalue(e, dotaddr);
				c = getch();
				if (c == '\n') {
					print("%08lx", val);
					print("\n");
					continue;
				}
				ungetch(c);
				if (patch(space, val))
					continue;
				goto skip;
			}
			if (c != '\n') {
				p = dfmt[space];
				while (c != '\n') {
					*p++ = c;
					c = getch();
				}
				*p++ = '\0';
			}
			save = dotaddr;
			val = lastaddr;
			dotaddr = evalue(e, lastaddr);
			display(space, dfmt[space], (int)evalue2(&e[1], 1L));
			if (space != 2)
				dotspace = space;
			else {
				dotaddr = save;
				lastaddr = val;
			}
			continue;
		case ':':
			runreq = 0;
			if (colon(e))
				continue;
			return;
		default:
			runreq = 0;
			synerr("Syntax error");
			goto skip;
		}
skip:
		do {
			c = getch();
		} while (c != '\n');
	}
}

/* 0x2292 patch(space, addr) — `=v,v,...': patch up to 10 values of the current datum size */
patch(space, addr)
int space;
long addr;
{
	register int i;
	register char *p;
	struct opnd e[NEXPR];
	long lv;
	int wv;
	char b3[3];
	char bv;
	int c;

	if (exprlist(e) == 0)
		return (0);
	c = getch();
	if (c != '\n')
		return (0);
	for (i = 0; i < NEXPR; i++, addr += dsize) {
		if (eempty(&e[i]))
			continue;
		lv = evalue2(&e[i], 0L);
		switch (dsize) {
		case 1:
			bv = lv;
			p = &bv;
			break;
		case 2:
			wv = lv;
			p = (char *)&wv;
			break;
		case 3:
			ltol3(b3, &lv, 1);
			p = b3;
			break;
		case 4:
			p = (char *)&lv;
			break;
		default:
			synerr("Bad change type");
			return (1);
		}
		dot = addr;
		if (wrmem(space, p, dsize) == 0) {
			synerr("Cannot change value");
			return (1);
		}
	}
	if (space == 2)
		getregs();
	return (1);
}

/* 0x240c shellcmd() — `!': hand the rest of the line to the shell */
shellcmd()
{
	register char *p;
	register int c;

	p = linebuf;
	while ((c = getch()) != '\n')
		*p++ = c;
	*p++ = '\0';
	system(linebuf);
	intrseen();
	printf("!\n");
}

/* 0x2468 display(space, fmt, count) — `?': display count items in format fmt from `.', line-buffered */
display(space, fmt, count)
int space;
char *fmt;
int count;
{
	register int c;
	register int type;		/* datum letter, `w' if none */
	register int rep;
	register char *out;		/* fill pointer in linebuf */
	register char *np;
	long start;			/* `.' at the start of the format */
	long item;			/* address of the item being formatted */
	long line;			/* address shown at the head of the line */
	char *fp;
	char *limit;
	int radix;			/* d, o, u or x; 0 = none given */

	rep = 1;
	radix = 0;
	dot = dotaddr;
	line = dotaddr;
	item = dotaddr;
	start = dotaddr;
	out = linebuf;
	limit = &linebuf[0x40];
	while (count-- != 0) {
		fp = fmt;
		for (;;) {
			c = *fp++;
			if (isascii(c) && isdigit(c)) {
				rep = 0;
				do {
					rep = rep * 10 + c - '0';
					c = *fp++;
				} while (isascii(c) && isdigit(c));
				fp--;
				continue;
			}
			switch (c) {
			case '+':
				dot += rep;
				rep = 1;
				continue;
			case '-':
				dot -= rep;
				rep = 1;
				continue;
			case '^':
				dot = start;
				continue;
			case 'd':
			case 'o':
			case 'u':
			case 'x':
				radix = c;
				continue;
			case 'n':
				*out = '\0';
				showline(line);
				out = linebuf;
				line = dot;
				continue;
			}
			type = c;
			if (type == 0 && radix == 0)
				break;
			if (type == 0)
				type = 'w';
			if (radix == 0)
				radix = 'x';
			start = dot;
			while (rep-- != 0) {
				if (intrseen())
					return (1);
				*out++ = ' ';
				item = dot;
				np = (char *)fmtitem(out, space, type, radix, rdmem);
				if (np == NULL) {
					out--;
					*out = '\0';
					if (out != linebuf)
						showline(line);
					synerr("Addressing error");
					return (0);
				}
				if (np <= limit)
					out = np;
				else {
					out--;
					*out = '\0';
					showline(line);
					*np = '\0';
					*out = ' ';
					np = out;
					out = linebuf;
					while (*np)
						*out++ = *np++;
					line = item;
				}
				if (c == 'i') {
					*out++ = '\0';
					showline(line);
					out = linebuf;
					line = dot;
				}
			}
			rep = 1;
			radix = 0;
			if (c == 0)
				break;
		}
		lastaddr = dot;
	}
	if (out != linebuf) {
		*out = '\0';
		showline(line);
	}
	return (1);
}

/* 0x2742 showline(addr) — print one display line: "%08lx  <line buffer>" */
showline(addr)
long addr;
{
	dotaddr = addr;
	print("%08lx", dotaddr);
	print("  %s\n", linebuf);
}
