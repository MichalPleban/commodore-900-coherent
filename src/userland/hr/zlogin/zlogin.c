/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * zlogin.c - graphical login greeter for the hi-res console (/etc/zlogin).
 *
 * Spawned by /etc/getty when it finds itself on the hi-res console (the
 * TIOCGTERM answer is "vt100" and the VRAM tail probes as real RAM), instead
 * of the textual "Coherent login:" prompt.  Draws a login panel straight into
 * the framebuffer -- no window server, no /drv/hr, no clgfx: like gfxtest,
 * it links only libhrgfx.a + globals.o + libc.a and writes VRAM directly
 * (the bitmap segments 0x3A/0x3B and the tail 0x38 are mapped user-accessible
 * in every process).  Layer.o is deliberately NOT used: its memfail() hangs
 * the machine on an out-of-memory, and a full-screen owner has nothing to
 * clip against -- every fill and glyph below goes through bitblt alone.
 *
 * Input is the console tty itself, in RAW mode with echo off.  /drv/hr is
 * NOT loaded: hrtty keeps the keyboard, so there is no mouse and no sprite --
 * a name + password box needs neither -- and no crash here can ever leave
 * the keyboard vector stolen and the machine deaf.
 *
 * On a successful login it does what /bin/login does (utmp/wtmp, chown of
 * the console, HOME/TERM environment, setgid/setuid) and then execs the
 * zview desktop as the session.  When zview quits, this pid is long gone
 * (exec chain getty -> zlogin -> zview), so init respawns getty on the
 * console and the login panel comes back: that is the logout loop.
 *
 * Anything unrecoverable (no card, no font) falls back to the textual
 * prompt: exec /etc/getty with a third argument "text", which tells getty
 * not to try the graphical path again (the loop guard).
 *
 * NOTE stdout/stderr are pointed at /dev/null before the first blit: there
 * is no console/graphics handshake (GUI.md sec 2.6), so any console write --
 * including the engine's own debug printf()s -- would scribble glyphs over
 * the panel, exactly the reason zview null-routes its own fds.
 */
#include <stdio.h>
#include <pwd.h>
#include <sgtty.h>
#include <signal.h>
#include <utmp.h>
#include <dir.h>
#include <machine.h>
#include <chars.h>
#include "smgr.h"
#include "shmem.h"
#include "hrdlg.h"

#define CONSOLE	"/dev/console"
#define ZVIEW	"/usr/hr/bin/zview"
#define GETTY	"/etc/getty"
#define UIFONT	"/usr/hr/fonts/gacha.b.hf"

#define PASSLEN	13		/* length of encrypted passwords (login.c) */
#define ACCNAME	"remacc"	/* remote-access pseudo user: no direct login */

/* Panel geometry.  The card is the dialog-style box (1px border, WD_SHADOW
 * drop shadow); everything inside is card-relative and laid out in whole
 * FUI cells (9x16).  Field boxes are DLG_THPAD-padded like a DW_TEXT. */
#define CARDW	320
#define CARDH	140
#define LBLX	12		/* label column, card-relative           */
#define FLDX	100		/* field box column                      */
#define FLDW	(CARDW - FLDX - DLG_MARG)
#define FLDH	22		/* 16 px text + 2*DLG_THPAD              */
#define TITLEY	12
#define NAMEY	44
#define PASSY	78
#define MSGY	112

#define NAMEMAX	DIRSIZ		/* utmp ut_name is DIRSIZ; also fits the box */
#define PASSMAX	20		/* fits the box; crypt() reads 8 anyway      */

extern BITMAP	display;
extern int	*texture[];
extern int	*screen_addr();
extern int	words_between();
extern char	*crypt();
extern struct passwd	*getpwnam();

/* The UI font, read from its .hf file into a private buffer -- NOT into the
 * VRAM tail: the tail belongs to the server, and zview re-stamps it from
 * scratch when the session starts.  A .hf file IS an HRFONT (shmem.h);
 * gacha.b.hf is 3048 bytes. */
static char	fontbuf[3200];
#define FONT	((HRFONT *)fontbuf)

static int	cardx, cardy;		/* card origin, screen coords     */
static char	name[NAMEMAX+1];
static char	pass[PASSMAX+1];
static int	nlen, plen;
static int	focus;			/* 0 = name field, 1 = password   */
static int	msgup;			/* a message line is showing      */

static struct sgttyb	savtty;		/* console modes as getty left them */

/* Default cooked modes for the session, same values /bin/login sets. */
static struct sgttyb	cooked = {
	B9600, B9600, '\b', CTRLU, EVENP|ODDP|CRMOD|ECHO|XTABS|CRT
};
static struct tchars	tchr = {
	CTRLC, FS, CTRLQ, CTRLS, CTRLD, -1
};

static char	*env[] = {
	"PATH=:/bin",
	0,			/* HOME= (filled in) */
	0,			/* TERM= (filled in) */
	0
};

/* Is there real RAM behind the tail?  Same write/read-back probe as
 * clgfx/hrsel.c hr_selok(), on the same scratch word: with no hi-res card
 * segment 0x38 has no responder and the store is dropped. */
static
probe()
{
	register short *p;

	p = (short *)(HRTAIL + SHM_SELPROBE);
	*p = 0x1234;
	if ( *p != 0x1234 )
		return 0;
	*p = 0x4321;
	return *p == 0x4321;
}

/* Fill a rectangle on screen (zview's srvfill, the working blit path).
 * op L_FALSE = black, L_TRUE + a texture = paint that pattern. */
static
fill(r, patidx, op)
RECT r;
{
	BLTSTRUCT blt;
	BITMAP s;

	if ( r.corner.x <= r.origin.x || r.corner.y <= r.origin.y )
		return;
	s.rect = r;
	s.width = 16 * words_between(r.origin.x, r.corner.x);
	s.base = screen_addr(r.origin.x, r.origin.y);
	blt.src = &s;
	blt.sp = r.origin;
	blt.dst = &display;
	blt.dr = r;
	blt.op = op;
	blt.pat = texture[patidx];
	bitblt(&blt, 1, 0);
}

static
frect(x0, y0, x1, y1, patidx, op)
{
	RECT r;

	r.origin.x = x0;  r.origin.y = y0;
	r.corner.x = x1;  r.corner.y = y1;
	fill(r, patidx, op);
}

/* 1-px black border around rect r (zview's dlg_border). */
static
border(r)
RECT r;
{
	RECT e;

	e = r; e.corner.y = r.origin.y + 1;   fill(e, 0, L_FALSE);
	e = r; e.origin.y = r.corner.y - 1;   fill(e, 0, L_FALSE);
	e = r; e.corner.x = r.origin.x + 1;   fill(e, 0, L_FALSE);
	e = r; e.origin.x = r.corner.x - 1;   fill(e, 0, L_FALSE);
}

/* Window-style stepped drop shadow (zview's dlg_shadow): r INCLUDES the
 * d-px shadow margin right + bottom. */
static
shadow(r, d)
RECT r;
{
	RECT card, e;
	int k;

	card = r;
	card.corner.x -= d;
	card.corner.y -= d;
	e = r;  e.origin.x = card.corner.x;  fill(e, 0, L_TRUE);
	e = r;  e.origin.y = card.corner.y;  fill(e, 0, L_TRUE);
	for ( k = 0; k < d; k++ )
	{
		e = r;					/* right band */
		e.origin.x = card.corner.x + k;   e.corner.x = e.origin.x + 1;
		e.origin.y = r.origin.y + k;
		e.corner.y = r.corner.y - (d - 1) + k;
		fill(e, 0, L_FALSE);
		e = r;					/* bottom band */
		e.origin.y = card.corner.y + k;   e.corner.y = e.origin.y + 1;
		e.origin.x = r.origin.x + k;
		e.corner.x = r.corner.x - (d - 1) + k;
		fill(e, 0, L_FALSE);
	}
}

/* One glyph, cell top-left at (gx,gy) -- zview's glyph1 pointed at the
 * private font copy.  Ink is stored 1 (white-on-black), so L_NSRC paints
 * black ink on a white cell.  No clipping: this program owns the screen. */
static
glyph(gx, gy, c)
{
	register HRFONT *f;
	BLTSTRUCT blt;
	BITMAP src;
	RECT dr;
	int gi;

	f = FONT;
	if ( c < f->first || c >= f->first + f->nch )
		return;
	gi = c - f->first;
	src.base = (int *)&f->bits[gi * f->cellh];
	src.width = 16;				/* one word per glyph row */
	src.rect.origin.x = 0;   src.rect.origin.y = 0;
	src.rect.corner.x = f->cellw;  src.rect.corner.y = f->cellh;
	dr.origin.x = gx;  dr.origin.y = gy;
	dr.corner.x = gx + f->cellw;
	dr.corner.y = gy + f->cellh;
	blt.src = &src;
	blt.dst = &display;
	blt.op = L_NSRC;
	blt.pat = texture[0];
	blt.dr = dr;
	blt.sp.x = 0;
	blt.sp.y = 0;
	bitblt(&blt, 1, 0);
}

static
text(px, py, s)
register char *s;
{
	register int c;

	for ( ; (c = *s & 0xff) != 0; s++, px += FONT->cellw )
		if ( c >= 0x20 && c <= 0x7e )
			glyph(px, py, c);
}

/* Centred text between x0..x1: the +1 is the FUI glyph offset (the glyphs
 * sit a pixel high-left in their cells, see dlg_button). */
static
ctext(x0, x1, py, s)
char *s;
{
	text(x0 + (x1 - x0 - strlen(s) * FONT->cellw) / 2 + 1, py + 1, s);
}

/* Redraw one entry field: white interior, black border, its text (the
 * password as stars) and, when it has the focus, the caret bar. */
static
drawfield(i)
{
	RECT r;
	char stars[PASSMAX+1];
	register char *s;
	register int n, tx;

	r.origin.x = cardx + FLDX;
	r.origin.y = cardy + (i == 0 ? NAMEY : PASSY);
	r.corner.x = r.origin.x + FLDW;
	r.corner.y = r.origin.y + FLDH;
	fill(r, 0, L_TRUE);
	border(r);
	if ( i == 0 )
	{
		s = name;
		n = nlen;
	}
	else
	{
		for ( n = 0; n < plen; n++ )
			stars[n] = '*';
		stars[plen] = '\0';
		s = stars;
		n = plen;
	}
	tx = r.origin.x + 1 + DLG_THPAD;
	text(tx + 1, r.origin.y + (FLDH - FONT->cellh) / 2 + 1, s);
	if ( i == focus )
		frect(tx + n * FONT->cellw + 1, r.origin.y + DLG_THPAD,
		      tx + n * FONT->cellw + 3, r.corner.y - DLG_THPAD,
		      0, L_FALSE);
}

/* The message line (centred, card-wide).  An empty string clears it. */
static
message(s)
char *s;
{
	frect(cardx + 1, cardy + MSGY, cardx + CARDW - 1, cardy + MSGY + 16,
	      0, L_TRUE);
	ctext(cardx, cardx + CARDW, cardy + MSGY, s);
	msgup = (*s != '\0');
}

/* Paint the whole screen: desktop dither, then the card with its chrome.
 * The dither also erases whatever the text console left, including its
 * parked XOR block cursor. */
static
drawpanel()
{
	RECT r;
	static char title[32];

	fill(display.rect, 10, L_TRUE);		/* OFF_WHITE desktop dither */

	r.origin.x = cardx;  r.origin.y = cardy;
	r.corner.x = cardx + CARDW + WD_SHADOW;
	r.corner.y = cardy + CARDH + WD_SHADOW;
	shadow(r, WD_SHADOW);
	r.corner.x -= WD_SHADOW;
	r.corner.y -= WD_SHADOW;
	fill(r, 0, L_TRUE);
	border(r);

	strcpy(title, "OpenCoherent ");
	strcat(title, VERSION);
	ctext(cardx, cardx + CARDW, cardy + TITLEY, title);
	text(cardx + LBLX + 1, cardy + NAMEY + (FLDH - 16) / 2 + 1, "Login:");
	text(cardx + LBLX + 1, cardy + PASSY + (FLDH - 16) / 2 + 1, "Password:");
	drawfield(0);
	drawfield(1);
}

/* RAW, echo off: every byte comes straight to read(2) and nothing is
 * echoed as console glyphs over the panel.  Speeds and editing characters
 * are kept from whatever getty left. */
static
rawtty()
{
	struct sgttyb sg;

	sg = savtty;
	sg.sg_flags = RAW;
	ioctl(0, TIOCSETP, &sg);
}

/* Fall back to the textual login: restore the console (modes AND fds 1/2,
 * which may already point at /dev/null) and exec getty with the "text"
 * argument so it does not bounce straight back here. */
static
giveup()
{
	ioctl(0, TIOCSETP, &savtty);
	dup2(0, 1);
	dup2(0, 2);
	execl(GETTY, "getty", "P", "text", NULL);
	exit(1);				/* init respawns getty */
}

/* Accounting entry for wtmp / the failed log, plus the /etc/utmp slot on
 * success -- copied from /bin/login's setutmp. */
static
setutmp(tty, username, filep, success)
char *tty, *username, *filep;
{
	time_t time();
	struct utmp utmp;
	struct utmp spare;
	size_t freeslot = -1, slot = 0;
	register ufd;

	utmp.ut_time = time((time_t)0);
	strncpy(utmp.ut_line, tty+5, 8);
	strncpy(utmp.ut_name, username, DIRSIZ);
	if ((ufd = open(filep, 1)) >= 0) {
		lseek(ufd, 0L, 2);
		write(ufd, &utmp, sizeof (utmp));
		close(ufd);
	}
	if (!success)
		return;

	if ((ufd = open("/etc/utmp", 2)) >= 0) {
		while (read(ufd, &spare, sizeof (spare)) == sizeof (spare)) {
			if (spare.ut_line[0] == '\0')
				freeslot = slot;
			else if (strncmp(spare.ut_line, utmp.ut_line, 8) == 0) {
				freeslot = slot;
				break;
			}
			slot += sizeof (utmp);
		}
		if (freeslot >= 0)
			lseek(ufd, freeslot, 0);
		write(ufd, &utmp, sizeof (utmp));
		close (ufd);
	}
}

/* Check the typed name/password against /etc/passwd, same acceptance rules
 * as /bin/login: an empty pw_passwd needs no password at all; a set one
 * must crypt-match and be the full PASSLEN; the remote-access pseudo user
 * cannot log in directly.  Returns the passwd entry or NULL. */
static struct passwd *
auth()
{
	register struct passwd *pwp;
	register char *cp;

	setpwent();
	pwp = getpwnam(name);
	if ( pwp == (struct passwd *)0 || strcmp(name, ACCNAME) == 0 )
	{
		crypt(pass, "xx");		/* burn the time anyway */
		return (struct passwd *)0;
	}
	if ( pwp->pw_passwd[0] == '\0' )
		return pwp;
	cp = crypt(pass, pwp->pw_passwd);
	if ( strcmp(cp, pwp->pw_passwd) == 0 &&
	     strlen(pwp->pw_passwd) == PASSLEN )
		return pwp;
	return (struct passwd *)0;
}

/* The successful login: /bin/login's bookkeeping, then exec the desktop as
 * the session.  Returns only on a pre-privilege failure (no home dir);
 * after setuid the only way out is exit -> init respawns the greeter. */
static
session(pwp)
register struct passwd *pwp;
{
	static char homebuf[5+128];		/* "HOME=" + pw_dir */
	static char termbuf[5+TERMSZ];		/* "TERM=" + name   */
	register int i;

	if ( chdir(pwp->pw_dir) < 0 )
	{
		message("No home directory");
		return;
	}
	setutmp(CONSOLE, pwp->pw_name, "/usr/adm/wtmp", 1);
	chown(CONSOLE, pwp->pw_uid, pwp->pw_gid);
	chmod(CONSOLE, 0700);

	strcpy(homebuf, "HOME=");
	strcat(homebuf, pwp->pw_dir);
	env[1] = homebuf;
	strcpy(termbuf, "TERM=");
	if ( ioctl(0, TIOCGTERM, &termbuf[5]) >= 0 )
	{
		termbuf[5+TERMSZ-1] = '\0';
		env[2] = termbuf;
	}

	setgid(pwp->pw_gid);
	setuid(pwp->pw_uid);
	endpwent();

	/* Cooked console for the session and whatever follows it. */
	ioctl(0, TIOCSETP, &cooked);
	ioctl(0, TIOCSETC, &tchr);

	for ( i = 3; close(i) >= 0; i++ )
		;
	execle(ZVIEW, "zview", NULL, env);
	exit(1);				/* no desktop: respawn and retry */
}

main()
{
	register int c;
	register struct passwd *pwp;
	int fd, n, esc;
	char ch;

	for ( c = 1; c <= NSIG; c++ )
		signal(c, SIG_DFL);
	ioctl(0, TIOCGETP, &savtty);

	/* Everything that can fail, before the first pixel: with no card or
	 * no font the textual prompt is the right answer. */
	if ( !probe() )
		giveup();
	if ( (fd = open(UIFONT, 0)) < 0 )
		giveup();
	n = read(fd, fontbuf, sizeof(fontbuf));
	close(fd);
	if ( n < 8 || n >= sizeof(fontbuf) )	/* empty or not gacha.b.hf */
		giveup();

	/* From here on the framebuffer is ours; no console write may happen
	 * (the engine's stray printf()s included), so null-route 1 and 2. */
	if ( (fd = open("/dev/null", 2)) >= 0 )
	{
		dup2(fd, 1);
		dup2(fd, 2);
		if ( fd > 2 )
			close(fd);
	}

	display.base = SEG0;
	display.rect.origin.x = XMIN;
	display.rect.origin.y = YMIN;
	display.rect.corner.x = XMAX;
	display.rect.corner.y = YMAX;
	display.width = DIS_WIDTH;

	cardx = ((XMAX - CARDW - WD_SHADOW) / 2) & ~0x0f;
	cardy = (YMAX - CARDH - WD_SHADOW) / 2;

	drawpanel();
	rawtty();

	esc = 0;
	for ( ;; )
	{
		if ( read(0, &ch, 1) != 1 )
			giveup();		/* console gone: not our fight */
		c = ch & 0x7f;
		if ( esc )			/* drop ESC + one byte, the    */
		{				/* sh line-reader idiom, so a  */
			esc = 0;		/* stray arrow types nothing   */
			continue;
		}
		if ( c == 033 )
		{
			esc = 1;
			continue;
		}
		if ( c == '\r' || c == '\n' )
		{
			if ( focus == 0 )
			{
				if ( nlen == 0 )
					continue;
				focus = 1;
				drawfield(0);
				drawfield(1);
				continue;
			}
			pwp = auth();
			if ( pwp != (struct passwd *)0 )
				session(pwp);	/* returns only on failure */
			else
			{
				setutmp(CONSOLE, name, "/usr/adm/failed", 0);
				message("Login incorrect");
			}
			nlen = plen = 0;
			name[0] = pass[0] = '\0';
			focus = 0;
			drawfield(0);
			drawfield(1);
			continue;
		}
		if ( msgup )
			message("");
		if ( c == '\t' )
		{
			focus = !focus;
			drawfield(0);
			drawfield(1);
			continue;
		}
		if ( c == '\b' || c == 0177 )
		{
			if ( focus == 0 && nlen > 0 )
				name[--nlen] = '\0';
			else if ( focus == 1 && plen > 0 )
				pass[--plen] = '\0';
			drawfield(focus);
			continue;
		}
		if ( c == CTRLU )
		{
			if ( focus == 0 )
				name[nlen = 0] = '\0';
			else
				pass[plen = 0] = '\0';
			drawfield(focus);
			continue;
		}
		if ( c < ' ' )
			continue;
		if ( focus == 0 && nlen < NAMEMAX )
		{
			name[nlen++] = c;
			name[nlen] = '\0';
			drawfield(0);
		}
		else if ( focus == 1 && plen < PASSMAX )
		{
			pass[plen++] = c;
			pass[plen] = '\0';
			drawfield(1);
		}
	}
}
