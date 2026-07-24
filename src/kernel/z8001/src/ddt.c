/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Mini ddt.
 * This routine gets dragged in
 * with the system. It is used to help
 * debug the system when the ROM is
 * full of bootstrap.
 * This code has a token understanding of things segmented.
 *
 * ?1	Illegal command.
 * ?2	Syntax error of any kind.
 * ?3	Odd address.
 */

#include <coherent.h>
#include <buf.h>
#include <proc.h>
#include <seg.h>

#define	NCBUF	(4*64)
#define	NREG	sizeof(regs)/sizeof(regs[0])
#define	NNAME	10
#define	DEL	0x7F		/* Ascii delete */
#define	I	0		/* Integer length */
#define	L	1		/* Long length */
#define	putihex(i)	puthex((long)(i), 12)
#define	putlhex(l)	puthex(l, 28)

BUF	dumpbuf;
static	char	cbuf[NCBUF];
extern	int	*ddtregp;

struct	mmu	{
	unsigned	base;
	unsigned char	len;
	unsigned char	attr;
};

/*
 * Register names.
 */
static	struct	regs {
	char	r_off;
	char	r_len;
	char	r_name[4];
}	regs[] = {
	OR0, I, "r0",
	OR1, I, "r1",
	OR2, I, "r2",
	OR3, I, "r3",
	OR4, I, "r4",
	OR5, I, "r5",
	OR6, I, "r6",
	OR7, I, "r7",
	OR8, I, "r8",
	OR9, I, "r9",
	OR10, I, "r10",
	OR11, I, "r11",
	OR12, I, "r12",
	OR13, I, "r13",
	OS14, I, "r14",
	OS15, I, "r15",
	OR14, L, "nsp",
	OFCW, I, "fcw",
	OPC, L, "pc",
};

/*
 * Signal names.
 */
static	char	*signame[] = {
	"ITS V01",
	"Hangup",
	"Interrupt",
	"Quit",
	"Alarm",
	"Termination",
	"Restart",
	"System call",
	"Pipe broken",
	"Kill",
	"Breakpoint",
	"Segment trap",
	"Unimplemented instruction",
	"Privileged instruction",
	"Non-vectored interrupt",
	"Non-maskable Interrupt",
};

/*
 * Kill bugs.
 * Called from the mch and from the
 * user with a signal code. A code of 0
 * means user call.
 */
ddt(sig)
{
	long ladd;
	register char *cp, *np;
	register c, v;
	register struct regs *rp;
	register PROC *pp;
	register struct mmu *mp;
	int nword, address, nwonl;
	int pflag;
	unsigned data;
	saddr_t m;
	int s;
	long l1, l2, l3, ln;
	char name[NNAME];

	ddtregp -= OR14;		/* Displacement to top of registers */
	s = sphi();
	printf("%p: %s\n", *(long *)(ddtregp+OPC), signame[sig]);
	for (;;) {
		getl();
		cp = &cbuf[0];
		while ((c = *cp++) == ' ')
			;
		if (c == '\n')
			continue;
		switch (c) {

		case 'c':
			spl(s);
			return;

		case 'd':
			pflag = 0;
			if (*cp == 'p') {
				pflag++;
				cp++;
			}
			while ((c = *cp++) == ' ')
				;
			if ((v = hexdig(c)) < 0) {
				diag('2');
				break;
			}
			ladd = 0;
			do {
				ladd = (ladd<<4) + v;
			} while ((v = hexdig(c = *cp++)) >= 0);
			if ((ladd&01) != 0) {
				diag('3');
				break;
			}
			for (;;) {
				while (c == ' ')
					c = *cp++;
				if (c == '\n')
					break;
				if ((v = hexdig(c)) < 0) {
					diag('2');
					break;
				}
				data = 0;
				do {
					data = (data<<4) + v;
				} while ((v = hexdig(c = *cp++)) >= 0);
				if (pflag)
					l1 = pfix(DBS, ladd); else
					l1 = ptov(ladd);
				kkcopy(&data, l1, sizeof data);
				ladd += sizeof (int);
			}
			break;

		case 'e':
			pflag = 0;
			if (*cp == 'p') {
				pflag++;
				cp++;
			}
			while ((c = *cp++) == ' ')
				;
			if ((v = hexdig(c)) < 0) {
				diag('2');
				break;
			}
			ladd = 0;
			do {
				ladd = (ladd<<4) + v;
			} while ((v = hexdig(c = *cp++)) >= 0);
			nword = 1;
			while (c == ' ')
				c = *cp++;
			if (c != '\n') {
				nword = 0;
				while ((v = hexdig(c)) >= 0) {
					nword = (nword<<4) + v;
					c = *cp++;
				}
			}
			if ((ladd&01) != 0) {
				diag('3');
				break;
			}
			nwonl = 0;
			while (nword--) {
				if (nwonl >= 8) {
					putchar('\n');
					nwonl = 0;
					if ((c = getchar()) == DEL)
						break;
				}
				if (nwonl == 0) {
					putlhex(ladd);
					putchar(' ');
					putchar('-');
				}
				putchar(' ');
				if (pflag)
					l1 = pfix(DBS, ladd); else
					l1 = ptov(ladd);
				kkcopy(l1, &data, sizeof data);
				putihex(data);
				ladd += sizeof (int);
				++nwonl;
			}
			if (nwonl != 0)
				putchar('\n');
			break;

		case 'f':
			while ((c = *cp++) == ' ')
				;
			if ((v = hexdig(c)) < 0) {
				diag('2');
				break;
			}
			address = 0;
			do {
				address = (address<<4) + v;
			} while ((v = hexdig(c = *cp++)) >= 0);
			nword = 0;
			while (c == ' ')
				c = *cp++;
			if (c != '\n') {
				nword = 0;
				while ((v = hexdig(c)) >= 0) {
					nword = (nword<<4) + v;
					c = *cp++;
				}
			}
			asave(m);
			for (v=0; v<1024; v++) {
				if (*((int *)(abase(v)+address)) == nword)
					printf("%x\n", v);
			}
			arest(m);
			break;

		case 'm':
			while ((c = *cp++) == ' ')
				;
			if (c == '\n') {
				diag('2');
				break;
			}
			np = &name[0];
			while (c!=' ' && c!='\n') {
				if (np < &name[NNAME-1])
					*np++ = c;
				c = *cp++;
			}
			*np = 0;
			while (c == ' ')
				c = *cp++;
			if ((v = hexdig(c)) < 0) {
				diag('2');
				break;
			}
			ln = 0;
			do {
				ln = (ln<<4) + v;
			} while ((v = hexdig(c = *cp++)) >= 0);
			for (rp = &regs[0]; rp < &regs[NREG]; rp++)
				if (eq(name, rp->r_name))
					break;
			if (rp == &regs[NREG]) {
				diag('2');
				break;
			}
			switch (rp->r_len) {
			case I:
				ddtregp[rp->r_off] = ln;
				break;

			case L:
				*(long *)(ddtregp+rp->r_off) = ln;
				break;
			}
			break;

		case 'p':
			for (pp=procq.p_nback; pp!=&procq; pp=pp->p_nback) {
				v = 0;
				for (c=0; c<NUSEG; c++) {
					if (pp->p_segp[c] == NULL)
						continue;
					v += pp->p_segp[c]->s_size;
				}
				printf("%d %d %dK %o %c %d %d (%p)\n",
					pp->p_pid, pp->p_ppid, ctokrd(v),
					pp->p_flags,
					"?SRZ"[pp->p_state],
					pp->p_sval, pp->p_rval,
					pp);
			}
			break;

		case 'r':
			for (c=0; c<NREG; ++c) {
				if ((c&03) != 0)
					putchar(' ');
				else if (c != 0)
					putchar('\n');
				rp = &regs[c];
				putmesg(rp->r_name);
				putchar(' ');
				if (rp->r_len == I)
					putihex(ddtregp[rp->r_off]); else
					putlhex(*(long *)(ddtregp+rp->r_off));
			}
			putchar('\n');
			break;

		case 's':
			l1 = 0;
			l2 = ctob((long)coretop);
			l3 = 0;
			while (l1 < l2) {
				register BUF *bp;

				bp = &dumpbuf;
				bp->b_flag = BFNTP;
				bp->b_req = BWRITE;
				bp->b_dev = swapdev;
				bp->b_bno = l3;
				ln = l2 - l1;
				c = ln>SCHUNK ? SCHUNK : ln;
				bp->b_count = c;
				bp->b_paddr = l1;
				printf("write(%u, %o, %d)\n",
					(int)bp->b_bno, (int)bp->b_paddr,
					bp->b_count);
				dblock(bp->b_dev, bp);
				splo();
				while ((bp->b_flag&BFNTP) != 0)
					;
				if (bp->b_err != 0) {
					printf("Error = %d\n", bp->b_err);
					break;
				}
				sphi();
				l1 += c;
				l3 += c/512;
			}
			break;

		case 'M':
			getmmu(0, 64, cbuf);
			mp = cbuf;
			for (c=0; c<64; c++) {
				printf("%x: %x %x %x", c, mp->base,
				    mp->attr, mp->len);
				mp++;
				if ((c & 03) == 03)
					putchar('\n'); else
					printf("     ");
			}
			break;

		default:
			diag('1');
		}
	}
}

/*
 * If the character `c' is a legal
 * hexadecimal digit then return the binary
 * value of the digit. If it is not legal
 * return -1.
 */
static
hexdig(c)
register c;
{
	if (c>='0' && c<='9')
		return (c - '0');
	if (c>='a' && c<='f')
		return (c - 'a' + 10);
	if (c>='A' && c<='F')
		return (c - 'A' + 10);
	return (-1);
}

/*
 * Get a command line.
 * Do some sort of erase and kill
 * processing. The command is packed into
 * the command buffer `cbuf'.
 */
static
getl()
{
	register char *cp;
	register c;

again:
	putchar('*');
	cp = &cbuf[0];
	for (;;) {
		c = getchar();
		if (c == '@') {
			putchar('\n');
			goto again;
		}
		if (c == '\b') {
			if (cp > &cbuf[0])
				--cp;
			continue;
		}
		if (c == '\n') {
			*cp = '\n';
			break;
		}
		if (cp < &cbuf[NCBUF-1])
			*cp++ = c;
	}
}

/*
 * Put out a string using
 * repeated calls to the put character
 * routine.
 */
static
putmesg(s)
register char *s;
{
	register c;

	while ((c = *s++) != '\0')
		putchar(c);
}

/*
 * Put out a long integer in
 * upper case hexadecimal ascii.
 */
static
puthex(l, s)
register long l;
register int s;
{
	register int c;

	do {
		c = (l >> s) & 0xF;
		c += '0';
		if (c > '9')
			c += 'A'-'0'-10;
		putchar(c);
	} while ((s -= 4) >= 0);
}

/*
 * Error routine.
 * The character `c' is the error
 * code. This is usually a digit but the
 * followers of Tom Duff may prefer a
 * letter.
 */
static
diag(c)
{
	putchar('?');
	putchar(c);
	putchar('\n');
}

/*
 * Compare two strings.
 */
static
eq(p1, p2)
register char *p1, *p2;
{
	register c;

	while ((c = *p1++) == *p2++) {
		if (c == '\0')
			return (1);
	}
	return (0);
}
