/*
 * zmail.c - a ZView mail client.
 *
 * The mail system it speaks is 7mail's (the command installed as /bin/mail):
 * a user's incoming mail is ONE mbox-format file /usr/spool/mail/<user>,
 * messages delimited by lines beginning "From <sender> <ctime>"; a body line
 * beginning "From " is quoted as ">From " by the sender.  Sending appends to
 * the recipient's spool file under the same advisory lock 7mail takes
 * (creat /tmp/maillock<uid>, spin while it exists), so zmail and mail can be
 * used together on the same mailbox.  Deleting marks a message and "Purge"
 * rewrites the spool without the marked ones -- 7mail's d + q, made explicit.
 *
 * A direct-render client (GUI.md Model A, like zedit): window layout, top to
 * bottom:
 *   - a row of BUTTONS (UI font): New, Reply, Delete, Purge while reading;
 *     Send, Cancel while composing -- with a message count on the right;
 *   - a scrollable LIST of messages (number, D flag, sender, date, subject),
 *     the selected one shown inverted; click a line to select it;
 *   - a scrollable CONTENT pane showing the selected message -- which becomes
 *     an EDITOR when composing (New/Reply): a HEADER STRIP with two text
 *     fields, To: and Subject:, sits above the body editor.  New starts with
 *     the To: field focused; Tab (or Enter, or ^N/down) moves To -> Subject ->
 *     body, ^P/up from the top of the body goes back to Subject, and a click
 *     focuses whatever it lands on.  Send reads the recipients and subject
 *     from the fields and delivers the body.
 * Both panes have the common vertical scrollbar (hrsbar) on the LEFT edge,
 * 16 px = one VRAM word, so the text grid keeps its byte alignment.
 *
 * The subject shown in the list is the message's "Subject: " line when one of
 * its first few body lines is that (zmail's own composer writes one), else its
 * first non-blank body line as a preview -- 7mail itself adds no headers.
 *
 * New mail is polled for on a SIGALRM tick (spool size change), so mail sent
 * to oneself appears in the list moments later, and deletions made by another
 * mail process are picked up too.
 *
 * Keys: ^P/^N select the previous/next message while reading (the arrows,
 * via zvpump's keypad map); ^Z/^V page the content pane; d toggles deletion.
 * Composing edits with the zedit subset: ^B ^F ^P ^N ^A ^E motion, ^Z ^V
 * paging, BS/^D/DEL delete, printable characters insert; a click places the
 * cursor and a middle-click pastes the PRIMARY selection.
 */
#include <stdio.h>
#include <types.h>
#include <stat.h>
#include <pwd.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrsbar.h"
#include "hrdlg.h"

extern char	*malloc();
extern char	*ctime();
extern long	ftell();
extern long	time();
extern struct passwd	*getpwnam(), *getpwuid();
extern char	*getlogin();

#define	SPOOLDIR	"/usr/spool/mail/"

/* View grid ceilings: the biggest full-screen window at the 8x15 cell. */
#define	MAXROWS	52
#define	MAXCOLS	126

/* The message table. */
#define	MAXMSG	200
#define	NFROM	16
#define	NDATE	14
#define	NSUBJ	40

/* The content buffer (one message, or the composition). */
#define	MAXCL	500
#define	MAXLL	240
#define	TABW	8

/* Button-bar metrics (UI font, dialog-button chrome). */
#define	BARH	32		/* bar height incl. its bottom rule       */
#define	BTNY	3		/* button top                             */
#define	BTNPAD	10		/* label side padding inside a button     */
#define	BTNGAP	14		/* between buttons                        */
#define	MAXBTN	5

#define	NADDR	8		/* most recipients on one To: line        */

HRAPP	me = { "Mail", "mail.icn", 0, 0, HRF_STRETCH | HRF_CONFIRM, 0, 0, 0 };

int	mywid;
int	cellw, cellh;		/* terminal-font cell (the two panes)     */
int	fcw, fch;		/* UI-font cell (buttons, bar text)       */
int	xpix;			/* text-grid offset right of the bars     */
int	contw, conth;		/* granted content size, px               */
int	ly0, lrows;		/* list pane: top px, visible rows        */
int	cy0, crows;		/* content pane: top px, visible rows     */
int	cols;			/* text columns in both panes             */

/* ---- who we are (7mail's setname) ---- */
int	myuid, mygid;
char	myname[25];
char	spoolname[64];

/* ---- the mailbox ---- */
FILE	*mfp;			/* open spool stream, NULL when no file   */
long	mseek[MAXMSG];		/* start of message i                     */
long	mend[MAXMSG];		/* end of message i                       */
char	mdel[MAXMSG];		/* 1 = marked deleted                     */
char	mfrom[MAXMSG][NFROM];
char	mdate[MAXMSG][NDATE];
char	msubj[MAXMSG][NSUBJ];
int	nmsg;
long	boxsize;		/* spool size as of the last scan         */
int	selmsg = -1;		/* selected message, -1 = none            */
int	ltop;			/* first visible list row                 */
long	loadseek = -1;		/* which message the content pane holds   */

/* ---- the content pane: viewed message, or the composition ---- */
char	*ln[MAXCL];		/* malloc'd NUL-terminated lines          */
int	nln;			/* line count (always >= 1)               */
int	ctop, offx;		/* view: first line, first display column */
int	composing;		/* 1 = the pane is an editor              */
int	edited;			/* 1 = the composition has been typed in  */
int	dotl, dotc;		/* compose cursor: line, byte offset      */

/* ---- the compose header: To / Subject text fields (dialog-field look) ---- */
#define	HDRH	26		/* strip height above the body editor     */
#define	FLDH	22		/* field box height (hrdlg DW_TEXT's)     */
char	hto[64];		/* the To: field                          */
char	hsub[64];		/* the Subject: field                     */
int	hdrfoc = 2;		/* 0 = To, 1 = Subject, 2 = the body      */
int	hdrdirty;		/* 1 = repaint the header strip           */
int	by0;			/* body rows top px (cy0 + strip if composing) */
int	tofx, tofw;		/* To: field box, as laid out by drawhdr()     */
int	sjfx, sjfw;		/* Subject: field box                          */

/* ---- rendering (zedit's diff scheme over both panes) ---- */
char	disp[MAXROWS][MAXCOLS];	/* what is on screen; 0 = needs paint     */
int	curon;			/* 1 = block cursor painted (inverted)    */
int	curc, curr;		/* view cell where it was painted         */
int	hlon;			/* 1 = list highlight painted             */
int	hlrow;			/* list row it is on                      */
int	chromedirty = 1;	/* 1 = repaint bar + rules wholesale      */

HRSBAR	sbl;			/* list scrollbar                         */
HRSBAR	sbc;			/* content scrollbar                      */
int	sblforce = 1, sbcforce = 1;

/* the button bar as laid out by drawbar(), for hit-testing */
int	nbtn;
int	bx[MAXBTN], bw[MAXBTN];
char	*blab[MAXBTN];
int	barmed = -1;		/* armed (pressed) button, -1 = none      */
int	barin;			/* 1 = pointer currently inside it        */

int	mailflag;		/* SIGALRM: time to poll the spool        */

char	wk[MAXLL + 2];		/* line work buffer                       */
static char vbuf[MAXCOLS];	/* view-row expansion buffer              */

/* ------------------------------------------------------------------ */
/* line storage (zedit's)                                             */
/* ------------------------------------------------------------------ */

static char *
lndup(s, n)
char *s;
{
	register char *p;
	register int i;

	if ( (p = malloc(n + 1)) == 0 )
		return 0;
	for ( i = 0; i < n; i++ )
		p[i] = s[i];
	p[n] = 0;
	return p;
}

static
setline(i, s, n)
char *s;
{
	char *p;

	if ( (p = lndup(s, n)) == 0 )
		return -1;
	free(ln[i]);
	ln[i] = p;
	return 0;
}

static
insline(i, s, n)
char *s;
{
	char *p;
	register int j;

	if ( nln >= MAXCL || (p = lndup(s, n)) == 0 )
		return -1;
	for ( j = nln; j > i; j-- )
		ln[j] = ln[j-1];
	ln[i] = p;
	nln++;
	return 0;
}

static
delline(i)
{
	register int j;

	free(ln[i]);
	for ( j = i; j < nln - 1; j++ )
		ln[j] = ln[j+1];
	nln--;
	return 0;
}

static
freebuf()
{
	register int i;

	for ( i = 0; i < nln; i++ )
		free(ln[i]);
	nln = 0;
	return 0;
}

static
newbuf()
{
	nln = 0;
	if ( (ln[0] = lndup("", 0)) != 0 )
		nln = 1;
	return 0;
}

static
addline(s, n)
char *s;
{
	char *p;

	if ( nln >= MAXCL || (p = lndup(s, n)) == 0 )
		return -1;
	ln[nln++] = p;
	return 0;
}

/* ------------------------------------------------------------------ */
/* display-column mapping (tab expansion, zedit's)                    */
/* ------------------------------------------------------------------ */

static
dispcol(li, ci)
{
	register char *p;
	register int dc, i;

	p = ln[li];
	dc = 0;
	for ( i = 0; i < ci && p[i]; i++ )
		dc = (p[i] == '\t') ? (dc / TABW + 1) * TABW : dc + 1;
	return dc;
}

static
byteat(li, dcw)
{
	register char *p;
	register int dc, i, nd;

	p = ln[li];
	dc = 0;
	for ( i = 0; p[i]; i++ )
	{
		nd = (p[i] == '\t') ? (dc / TABW + 1) * TABW : dc + 1;
		if ( dcw < nd )
			return i;
		dc = nd;
	}
	return i;
}

/* ------------------------------------------------------------------ */
/* the view: list rows + content rows -> one character grid           */
/* ------------------------------------------------------------------ */

/* Pixel y of view row r: rows 0..lrows-1 are the list, the rest content
 * (which starts below the To/Subject header strip while composing). */
static
rowy(r)
{
	if ( r < lrows )
		return ly0 + r * cellh;
	return by0 + (r - lrows) * cellh;
}

/* Text of view row r (cols cells, blank-padded), zedit's vrow shape. */
static char *
vrow(r)
{
	register char *p;
	register int i, dc, c;
	int li;
	char lbuf[MAXCOLS + 4];

	for ( i = 0; i < cols; i++ )
		vbuf[i] = ' ';
	if ( r < lrows )			/* a list line */
	{
		li = ltop + r;
		if ( li < 0 || li >= nmsg )
		{
			if ( li == 0 && nmsg == 0 )
			{
				p = "(no mail)";
				for ( i = 0; p[i] && i < cols; i++ )
					vbuf[i] = p[i];
			}
			return vbuf;
		}
		sprintf(lbuf, "%3d %c %-14.14s %-12.12s %.60s",
			li + 1, mdel[li] ? 'D' : ' ',
			mfrom[li], mdate[li], msubj[li]);
		for ( i = 0; lbuf[i] && i < cols; i++ )
			vbuf[i] = lbuf[i];
		return vbuf;
	}
	li = ctop + (r - lrows);		/* a content line */
	if ( li < 0 || li >= nln )
		return vbuf;
	p = ln[li];
	dc = 0;
	for ( i = 0; p[i]; i++ )
	{
		c = p[i] & 0xff;
		if ( c == '\t' )
			dc = (dc / TABW + 1) * TABW;
		else
		{
			if ( dc >= offx && dc - offx < cols )
				vbuf[dc - offx] =
				    (c < 0x20 || c > 0x7e) ? '?' : c;
			dc++;
		}
		if ( dc - offx >= cols )
			break;
	}
	return vbuf;
}

static
invalidate()
{
	register int r, c;

	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;
	curon = 0;
	hlon = 0;
	sblforce = 1;
	sbcforce = 1;
	chromedirty = 1;
	hdrdirty = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* overlays: list highlight + compose cursor (XOR, zedit's scheme)    */
/* ------------------------------------------------------------------ */

static
hldraw()
{
	int r;

	if ( selmsg < 0 )
		return 0;
	r = selmsg - ltop;
	if ( r < 0 || r >= lrows )
		return 0;
	cl_fillrect(xpix, ly0 + r * cellh, contw, ly0 + (r + 1) * cellh, 2);
	hlon = 1;  hlrow = r;
	return 0;
}

static
hlerase()
{
	if ( hlon )
	{
		cl_fillrect(xpix, ly0 + hlrow * cellh,
			    contw, ly0 + (hlrow + 1) * cellh, 2);
		hlon = 0;
	}
	return 0;
}

static
curdraw()
{
	int r, dc;

	if ( !composing || hdrfoc != 2 )	/* a header field holds the caret */
		return 0;
	r = dotl - ctop;
	if ( r < 0 || r >= crows )
		return 0;
	dc = dispcol(dotl, dotc) - offx;
	if ( dc < 0 || dc >= cols )
		return 0;
	cl_fillrect(xpix + dc * cellw, by0 + r * cellh,
		    xpix + (dc + 1) * cellw, by0 + (r + 1) * cellh, 2);
	curon = 1;  curc = dc;  curr = r;
	return 0;
}

static
curerase()
{
	if ( curon )
	{
		cl_fillrect(xpix + curc * cellw, by0 + curr * cellh,
			    xpix + (curc + 1) * cellw, by0 + (curr + 1) * cellh, 2);
		curon = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* the button bar                                                     */
/* ------------------------------------------------------------------ */

static
ndeleted()
{
	register int i, n;

	n = 0;
	for ( i = 0; i < nmsg; i++ )
		if ( mdel[i] )
			n++;
	return n;
}

/* Draw one button: white face, 1px border, 2px drop shadow, centred label
 * (the 9x16 UI glyphs sit 1px high-left in their cell, so centre +1,+1). */
static
drawbtn(i)
{
	int x, w, tx, ty;

	x = bx[i];  w = bw[i];
	cl_fillrect(x, BTNY, x + w, BTNY + DLG_BTNH, 1);
	cl_fillrect(x, BTNY, x + w, BTNY + 1, 0);
	cl_fillrect(x, BTNY + DLG_BTNH - 1, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x, BTNY, x + 1, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + w - 1, BTNY, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + 2, BTNY + DLG_BTNH, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	cl_fillrect(x + w, BTNY + 2, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	tx = x + (w - strlen(blab[i]) * fcw) / 2 + 1;
	ty = BTNY + (DLG_BTNH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, tx, ty, blab[i]);
	if ( i == barmed && barin )
		cl_fillrect(x + 1, BTNY + 1, x + w - 1, BTNY + DLG_BTNH - 1, 2);
	return 0;
}

/* The whole bar: lay the mode's buttons out, draw them, the status text on
 * the right, the rule under the bar and the rule between the panes. */
static
drawbar()
{
	static char *readbtns[] = { "New", "Reply", "Delete", "Purge" };
	static char *compbtns[] = { "Send", "Cancel" };
	char sbuf[40];
	register char **bp;
	register int i;
	int x, nd;

	cl_fillrect(0, 0, contw, BARH - 1, 1);
	cl_fillrect(0, BARH - 1, contw, BARH, 0);
	if ( composing )
	{
		bp = compbtns;
		nbtn = 2;
	}
	else
	{
		bp = readbtns;
		nbtn = 4;
	}
	x = 6;
	for ( i = 0; i < nbtn; i++ )
	{
		blab[i] = bp[i];
		bx[i] = x;
		bw[i] = strlen(bp[i]) * fcw + 2 * BTNPAD;
		drawbtn(i);
		x += bw[i] + BTNGAP;
	}
	if ( composing )
		strcpy(sbuf, "composing");
	else if ( nmsg == 0 )
		strcpy(sbuf, "no mail");
	else if ( (nd = ndeleted()) > 0 )
		sprintf(sbuf, "%d messages, %d deleted", nmsg, nd);
	else
		sprintf(sbuf, "%d message%s", nmsg, nmsg == 1 ? "" : "s");
	i = contw - 8 - strlen(sbuf) * fcw;
	if ( i > x )
		cl_ptext(SHM_FUI, i, BTNY + (DLG_BTNH - fch) / 2 + 1, sbuf);
	cl_fillrect(0, cy0 - 2, contw, cy0 - 1, 0);
	return 0;
}

/* Which button is under content pixel (x,y)?  -1 = none. */
static
btnhit(x, y)
{
	register int i;

	if ( y < BTNY || y >= BTNY + DLG_BTNH )
		return -1;
	for ( i = 0; i < nbtn; i++ )
		if ( x >= bx[i] && x < bx[i] + bw[i] )
			return i;
	return -1;
}

/* ------------------------------------------------------------------ */
/* the compose header strip (To / Subject fields)                     */
/* ------------------------------------------------------------------ */

/* One field box in the strip, hrdlg's DW_TEXT look: white body, 1 px border,
 * the text inset by the field pad, a caret bar after the last character when
 * the field holds the focus.  (The +1 on the text y is the FUI glyphs' usual
 * high-left-in-cell centring nudge.) */
static
drawfld(x, w, s, focus)
char *s;
{
	int y, ly, cx;

	y = cy0 + (HDRH - FLDH) / 2;
	cl_fillrect(x + 1, y + 1, x + w - 1, y + FLDH - 1, 1);
	cl_fillrect(x, y, x + w, y + 1, 0);
	cl_fillrect(x, y + FLDH - 1, x + w, y + FLDH, 0);
	cl_fillrect(x, y + 1, x + 1, y + FLDH - 1, 0);
	cl_fillrect(x + w - 1, y + 1, x + w, y + FLDH - 1, 0);
	ly = y + (FLDH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, x + DLG_THPAD + 2, ly, s);
	if ( focus )
	{
		cx = x + DLG_THPAD + 2 + strlen(s) * fcw;
		if ( cx < x + w - 2 )
			cl_fillrect(cx, ly, cx + 2, ly + fch, 0);
	}
	return 0;
}

/* The whole strip: labels + the two fields side by side, To: getting a fixed
 * sensible width and Subject: the rest of the line.  Lays the boxes out into
 * tofx/tofw / sjfx/sjfw for the click hit-test. */
static
drawhdr()
{
	int lx, ly;

	if ( !composing )
		return 0;
	cl_fillrect(0, cy0, contw, by0, 1);
	ly = cy0 + (HDRH - fch) / 2 + 1;
	lx = xpix + 4;
	cl_ptext(SHM_FUI, lx, ly, "To:");
	tofx = lx + 3 * fcw + 6;
	tofw = 22 * fcw + 2 * DLG_THPAD + 4;
	if ( tofx + tofw > contw - 8 )		/* a very narrow window */
		tofw = contw - 8 - tofx;
	lx = tofx + tofw + 14;
	cl_ptext(SHM_FUI, lx, ly, "Subject:");
	sjfx = lx + 8 * fcw + 6;
	sjfw = contw - 6 - sjfx;
	drawfld(tofx, tofw, hto, hdrfoc == 0);
	if ( sjfw > 4 * fcw )
		drawfld(sjfx, sjfw, hsub, hdrfoc == 1);
	return 0;
}

/* The pixel width a field has for text (mirror of drawfld's inset). */
static
fldroom(w)
{
	return w - 2 * DLG_THPAD - 6;
}

/* ------------------------------------------------------------------ */
/* painting                                                           */
/* ------------------------------------------------------------------ */

static
drawrun(r, c0, c1, vp)
char *vp;
{
	char buf[MAXCOLS + 1];
	int s, e, i, n, y;

	y = rowy(r);
	cl_fillrect(xpix + c0 * cellw, y,
		    xpix + c1 * cellw, y + cellh, 1);
	for ( s = c0; s < c1; s = e )
	{
		while ( s < c1 && vp[s] == ' ' )
			s++;
		if ( s >= c1 )
			break;
		for ( e = s; e < c1 && vp[e] != ' '; e++ )
			;
		n = 0;
		for ( i = s; i < e; i++ )
			buf[n++] = vp[i];
		buf[n] = 0;
		cl_ptext(SHM_FTERM, xpix + s * cellw, y, buf);
	}
	return 0;
}

static
clamptops()
{
	if ( ltop > nmsg - lrows )
		ltop = nmsg - lrows;
	if ( ltop < 0 )
		ltop = 0;
	if ( ctop > nln - crows )
		ctop = nln - crows;
	if ( ctop < 0 )
		ctop = 0;
	return 0;
}

static
flush()
{
	register int r, c;
	register char *vp;
	int c0, nrows;

	clamptops();
	cl_begin();
	hlerase();
	curerase();
	if ( chromedirty )
	{
		drawbar();
		chromedirty = 0;
		hdrdirty = 1;
	}
	if ( composing && hdrdirty )
		drawhdr();
	hdrdirty = 0;
	nrows = lrows + crows;
	for ( r = 0; r < nrows; r++ )
	{
		vp = vrow(r);
		c = 0;
		while ( c < cols )
		{
			if ( vp[c] == disp[r][c] )
			{
				c++;
				continue;
			}
			c0 = c;
			while ( c < cols && vp[c] != disp[r][c] )
			{
				disp[r][c] = vp[c];
				c++;
			}
			drawrun(r, c0, c, vp);
		}
	}
	hldraw();
	curdraw();
	sbl.sb_x = 0;
	sbl.sb_y = ly0;
	sbl.sb_h = lrows * cellh;
	sbl.sb_total = nmsg;
	sbl.sb_page = lrows;
	sbl.sb_pos = ltop;
	hr_sbdraw(&sbl, sblforce);
	sblforce = 0;
	sbc.sb_x = 0;
	sbc.sb_y = by0;
	sbc.sb_h = crows * cellh;
	sbc.sb_total = nln;
	sbc.sb_page = crows;
	sbc.sb_pos = ctop;
	hr_sbdraw(&sbc, sbcforce);
	sbcforce = 0;
	cl_end();
	return 0;
}

/* Derive the pane layout from the granted content size: the bar, then up to
 * 8 list rows (fewer in a short window, but at least 2), then the content
 * pane in what is left -- minus the To/Subject header strip while composing,
 * so startcompose/endcompose re-run this when they toggle the mode. */
static
layout()
{
	ly0 = BARH + 2;
	lrows = 8;
	while ( lrows > 2 && ly0 + lrows * cellh + 3 + 3 * cellh > conth )
		lrows--;
	cy0 = ly0 + lrows * cellh + 3;
	by0 = cy0 + (composing ? HDRH : 0);
	crows = (conth - by0) / cellh;
	cols = (contw - xpix) / cellw;
	if ( crows < 1 ) crows = 1;
	if ( lrows + crows > MAXROWS ) crows = MAXROWS - lrows;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( cols < 1 ) cols = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* who we are + the mailbox scan                                      */
/* ------------------------------------------------------------------ */

static
setname()
{
	register struct passwd *pwp;
	register char *np;

	myuid = getuid();
	if ( (np = getlogin()) != 0 && (pwp = getpwnam(np)) != 0 )
		myuid = pwp->pw_uid;
	else if ( (pwp = getpwuid(myuid)) == 0 )
		return -1;
	np = pwp->pw_name;
	mygid = pwp->pw_gid;
	strcpy(myname, np);
	strcpy(spoolname, SPOOLDIR);
	strcat(spoolname, np);
	return 0;
}

/* 7mail's mailbox lock: creat /tmp/maillock<uid>, spinning while it exists. */
char	locknm[32];
int	locked;

static
mlock(uid)
{
	register int fd, tries;

	sprintf(locknm, "/tmp/maillock%d", uid);
	for ( tries = 0; access(locknm, 0) == 0 && tries < 15; tries++ )
		sleep(1);
	if ( (fd = creat(locknm, 0)) >= 0 )
		close(fd);
	locked = 1;
	return 0;
}

static
munlock()
{
	if ( locked )
		unlink(locknm);
	locked = 0;
	return 0;
}

/* Scan the spool file into the message table.  Called at start-up, on the
 * poll tick when the size changed, and after a purge.  Deletion marks are
 * preserved for messages whose position did not move (append-only growth);
 * anything else gets a clean slate. */
static
rescan()
{
	static long oseek[MAXMSG];
	static char odel[MAXMSG];
	char b[256];
	register char *p;
	register int i;
	int onm, gotsubj, hline;
	long pos, osel;
	struct stat sb;

	onm = nmsg;
	for ( i = 0; i < nmsg; i++ )
	{
		oseek[i] = mseek[i];
		odel[i] = mdel[i];
	}
	osel = (selmsg >= 0 && selmsg < nmsg) ? mseek[selmsg] : -1;
	nmsg = 0;
	boxsize = 0;
	if ( mfp != 0 )
	{
		fclose(mfp);
		mfp = 0;
	}
	if ( stat(spoolname, &sb) < 0 || sb.st_size == 0 ||
	     (mfp = fopen(spoolname, "r")) == 0 )
	{
		selmsg = -1;
		ltop = 0;
		chromedirty = 1;
		return 0;
	}
	gotsubj = 1;
	hline = 0;
	for (;;)
	{
		pos = ftell(mfp);
		if ( fgets(b, sizeof(b), mfp) == 0 )
			break;
		if ( strncmp(b, "From ", 5) == 0 )
		{
			if ( nmsg >= MAXMSG )
				break;
			i = nmsg++;
			mseek[i] = pos;
			mend[i] = ftell(mfp);	/* grows as lines arrive */
			mdel[i] = 0;
			msubj[i][0] = 0;
			gotsubj = 0;
			hline = 0;
			/* "From sender Wed Aug  5 10:11:22 2026" */
			p = b + 5;
			for ( hline = 0; *p && *p != ' ' && *p != '\n' &&
			      hline < NFROM - 1; hline++ )
				mfrom[i][hline] = *p++;
			mfrom[i][hline] = 0;
			while ( *p == ' ' )
				p++;
			/* ctime chars 4..15: "Aug  5 10:11" */
			for ( hline = 0; hline < NDATE - 1 &&
			      p[4 + hline] != 0 && p[4 + hline] != '\n' &&
			      hline < 12; hline++ )
				mdate[i][hline] = p[4 + hline];
			mdate[i][hline] = 0;
			hline = 0;
			continue;
		}
		if ( nmsg == 0 )
			continue;		/* not mailbox format: skip */
		mend[nmsg - 1] = ftell(mfp);
		if ( gotsubj )
			continue;
		/* subject: a "Subject: " among the first few lines wins;
		 * else the first non-blank line (not the cc note) previews */
		p = b;
		hline++;
		if ( strncmp(p, "Subject: ", 9) == 0 && hline <= 4 )
		{
			for ( i = 0; p[9 + i] && p[9 + i] != '\n' &&
			      i < NSUBJ - 1; i++ )
				msubj[nmsg - 1][i] = p[9 + i];
			msubj[nmsg - 1][i] = 0;
			gotsubj = 1;
		}
		else if ( msubj[nmsg - 1][0] == 0 &&
			  *p != '\n' && strncmp(p, "(cc:", 4) != 0 )
		{
			for ( i = 0; p[i] && p[i] != '\n' && i < NSUBJ - 1; i++ )
				msubj[nmsg - 1][i] = p[i];
			msubj[nmsg - 1][i] = 0;
		}
	}
	boxsize = ftell(mfp);
	/* carry deletion marks over where the table did not move */
	for ( i = 0; i < nmsg && i < onm; i++ )
		if ( mseek[i] == oseek[i] )
			mdel[i] = odel[i];
	/* keep the selection on the same message if it is still there */
	selmsg = -1;
	for ( i = 0; i < nmsg; i++ )
		if ( mseek[i] == osel )
		{
			selmsg = i;
			break;
		}
	if ( selmsg < 0 && nmsg > 0 )
		selmsg = nmsg - 1;	/* newest, when the old one is gone */
	chromedirty = 1;
	return 0;
}

/* Load message i into the content pane (read mode).  Long lines are split;
 * a message longer than the buffer is truncated with a marker line. */
static
viewmsg(i)
{
	char b[256];
	register char *p;
	int wl, c;
	long pos;

	freebuf();
	offx = 0;
	ctop = 0;
	loadseek = -1;
	if ( i < 0 || i >= nmsg || mfp == 0 )
	{
		newbuf();
		return 0;
	}
	fseek(mfp, mseek[i], 0);
	wl = 0;
	pos = mseek[i];
	while ( pos < mend[i] && fgets(b, sizeof(b), mfp) != 0 )
	{
		pos = ftell(mfp);
		for ( p = b; *p; p++ )
		{
			c = *p & 0xff;
			if ( c == '\n' )
			{
				if ( addline(wk, wl) < 0 )
					goto full;
				wl = 0;
			}
			else if ( wl < MAXLL )
				wk[wl++] = c;
			else
			{
				if ( addline(wk, wl) < 0 )
					goto full;
				wl = 0;
				wk[wl++] = c;
			}
		}
	}
	if ( wl > 0 )
		addline(wk, wl);
full:
	if ( nln >= MAXCL )
		setline(nln - 1, "[message truncated]", 19);
	if ( nln == 0 )
		newbuf();
	loadseek = mseek[i];
	return 0;
}

static
select(i)
{
	if ( i < 0 || i >= nmsg || composing )
		return 0;
	selmsg = i;
	if ( selmsg < ltop )
		ltop = selmsg;
	if ( selmsg >= ltop + lrows )
		ltop = selmsg - lrows + 1;
	viewmsg(selmsg);
	return 0;
}

/* ------------------------------------------------------------------ */
/* dialogs                                                            */
/* ------------------------------------------------------------------ */

char	dmsg[44];

HRWIDGET mwg[] = {
    { DW_LABEL,   12,  16,   0,  0, dmsg },
    { DW_BUTTON, 150,  52,  70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NMWG	(sizeof(mwg) / sizeof(mwg[0]))

/* A one-line notice with an OK button. */
static
notice(msg)
char *msg;
{
	int w, h, r;

	strncpy(dmsg, msg, sizeof(dmsg) - 1);
	dmsg[sizeof(dmsg) - 1] = 0;
	w = strlen(dmsg) * 9 + 2 * DLG_MARG;
	if ( w < 240 ) w = 240;
	if ( w > 420 ) w = 420;
	h = 96;
	mwg[1].dw_x = (w - 70) / 2;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(mwg, NMWG);
	r = hr_dlgrun(mwg, NMWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return 0;
}

char	cfmsg[40];

HRWIDGET cwg[] = {
    { DW_LABEL,   12,  16,   0,  0, cfmsg },
    { DW_BUTTON,  40,  56,  90, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  56,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NCWG	(sizeof(cwg) / sizeof(cwg[0]))

static
confirm(msg)
char *msg;
{
	int w, h, r;

	strcpy(cfmsg, msg);
	w = 300;
	h = 100;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(cwg, NCWG);
	r = hr_dlgrun(cwg, NCWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return r == 1;
}

/* ------------------------------------------------------------------ */
/* sending (7mail's spool append, from in-memory lines)               */
/* ------------------------------------------------------------------ */

/* Deliver lines first..nln-1 to each user.  Returns 0, or -1 with the
 * reason in dmsg.  Every recipient is validated before anything is sent,
 * so a typo delivers to nobody rather than to half the list. */
static
deliver(users, nu, first)
char **users;
{
	char box[80];
	register struct passwd *pwp;
	register int u, l;
	int last;
	long now;
	FILE *xfp;

	for ( u = 0; u < nu; u++ )
		if ( getpwnam(users[u]) == 0 )
		{
			sprintf(dmsg, "No such user: %.20s", users[u]);
			return -1;
		}
	last = nln - 1;			/* trim trailing blank lines */
	while ( last >= first && ln[last][0] == 0 )
		last--;
	if ( last < first )
	{
		strcpy(dmsg, "The message is empty");
		return -1;
	}
	for ( u = 0; u < nu; u++ )
	{
		pwp = getpwnam(users[u]);
		sprintf(box, "%s%s", SPOOLDIR, users[u]);
		mlock(pwp->pw_uid);
		if ( (xfp = fopen(box, "a")) == 0 )
		{
			munlock();
			sprintf(dmsg, "Cannot send to %.20s", users[u]);
			return -1;
		}
		chown(box, pwp->pw_uid, pwp->pw_gid);
		time(&now);
		fprintf(xfp, "From %s %s", myname, ctime(&now));
		if ( nu > 1 )
		{
			fprintf(xfp, "(cc:");
			for ( l = 0; l < nu; l++ )
				fprintf(xfp, " %s", users[l]);
			fprintf(xfp, ")\n");
		}
		/* the Subject: field becomes the header line rescan() looks for
		 * (blank-separated from the body, the shape the old line-0
		 * composer produced) */
		if ( hsub[0] )
			fprintf(xfp, "Subject: %s\n\n", hsub);
		for ( l = first; l <= last; l++ )
		{
			if ( strncmp(ln[l], "From ", 5) == 0 )
				putc('>', xfp);
			fputs(ln[l], xfp);
			putc('\n', xfp);
		}
		putc('\n', xfp);	/* the blank separator line */
		fclose(xfp);
		munlock();
	}
	sync();
	return 0;
}

/* Parse the To: field into user names (split on blanks and commas). */
static
parseto(names, maxn)
char **names;
{
	static char tobuf[sizeof(hto)];
	register char *p;
	int n;

	strcpy(tobuf, hto);
	n = 0;
	p = tobuf;
	for (;;)
	{
		while ( *p == ' ' || *p == '\t' || *p == ',' )
			p++;
		if ( *p == 0 )
			break;
		if ( n >= maxn )
			break;
		names[n++] = p;
		while ( *p && *p != ' ' && *p != '\t' && *p != ',' )
			p++;
		if ( *p )
			*p++ = 0;
	}
	return n;
}

/* ------------------------------------------------------------------ */
/* compose-mode editing (the zedit subset)                            */
/* ------------------------------------------------------------------ */

static
fixview()
{
	int dc;

	if ( dotl < ctop )
		ctop = dotl;
	if ( dotl >= ctop + crows )
		ctop = dotl - crows + 1;
	if ( ctop < 0 )
		ctop = 0;
	dc = dispcol(dotl, dotc);
	if ( dc < offx )
		offx = dc;
	if ( dc >= offx + cols )
		offx = dc - cols + 1;
	if ( offx < 0 )
		offx = 0;
	return 0;
}

static
inschar(c)
{
	register char *p;
	register int i;
	int len;

	p = ln[dotl];
	len = strlen(p);
	if ( len >= MAXLL )
		return 0;
	for ( i = 0; i < dotc; i++ )
		wk[i] = p[i];
	wk[dotc] = c;
	for ( i = dotc; i < len; i++ )
		wk[i+1] = p[i];
	if ( setline(dotl, wk, len + 1) < 0 )
		return 0;
	dotc++;
	edited = 1;
	return 0;
}

static
donl()
{
	register char *p;
	register int i;
	int len;

	p = ln[dotl];
	len = strlen(p);
	if ( insline(dotl + 1, p + dotc, len - dotc) < 0 )
		return 0;
	p = ln[dotl];			/* insline moved pointers, not bytes */
	for ( i = 0; i < dotc; i++ )
		wk[i] = p[i];
	setline(dotl, wk, dotc);
	dotl++;
	dotc = 0;
	edited = 1;
	return 0;
}

static
delrange(l, i0, i1)
{
	register char *p;
	register int i;
	int len, n;

	if ( i1 <= i0 )
		return 0;
	p = ln[l];
	len = strlen(p);
	n = 0;
	for ( i = 0; i < i0; i++ )
		wk[n++] = p[i];
	for ( i = i1; i < len; i++ )
		wk[n++] = p[i];
	if ( setline(l, wk, n) == 0 )
		edited = 1;
	return 0;
}

static
joinln(l)
{
	register char *a, *b;
	register int i;
	int la, lb, n;

	if ( l >= nln - 1 )
		return 0;
	a = ln[l];  b = ln[l+1];
	la = strlen(a);  lb = strlen(b);
	if ( la + lb > MAXLL )
		return 0;
	n = 0;
	for ( i = 0; i < la; i++ )
		wk[n++] = a[i];
	for ( i = 0; i < lb; i++ )
		wk[n++] = b[i];
	if ( setline(l, wk, n) < 0 )
		return 0;
	delline(l + 1);
	edited = 1;
	return 1;
}

static
backspc()
{
	int nc;

	if ( dotc > 0 )
	{
		delrange(dotl, dotc - 1, dotc);
		dotc--;
	}
	else if ( dotl > 0 )
	{
		nc = strlen(ln[dotl - 1]);
		if ( joinln(dotl - 1) )
		{
			dotl--;
			dotc = nc;
		}
	}
	return 0;
}

static
delfwd()
{
	if ( dotc < strlen(ln[dotl]) )
		delrange(dotl, dotc, dotc + 1);
	else
		joinln(dotl);
	return 0;
}

static
mvleft()
{
	if ( dotc > 0 )
		dotc--;
	else if ( dotl > 0 )
	{
		dotl--;
		dotc = strlen(ln[dotl]);
	}
	return 0;
}

static
mvright()
{
	if ( dotc < strlen(ln[dotl]) )
		dotc++;
	else if ( dotl < nln - 1 )
	{
		dotl++;
		dotc = 0;
	}
	return 0;
}

static
mvup()
{
	int dc;

	if ( dotl == 0 )
	{
		dotc = 0;
		return 0;
	}
	dc = dispcol(dotl, dotc);
	dotl--;
	dotc = byteat(dotl, dc);
	return 0;
}

static
mvdown()
{
	int dc;

	if ( dotl >= nln - 1 )
	{
		dotc = strlen(ln[dotl]);
		return 0;
	}
	dc = dispcol(dotl, dotc);
	dotl++;
	dotc = byteat(dotl, dc);
	return 0;
}

/* Page the content pane; in compose mode the cursor moves with the view. */
static
pgmove(d)
{
	int dc, step;

	step = (crows > 1 ? crows - 1 : 1) * d;
	if ( composing )
	{
		dc = dispcol(dotl, dotc);
		dotl += step;
		if ( dotl < 0 ) dotl = 0;
		if ( dotl > nln - 1 ) dotl = nln - 1;
		dotc = byteat(dotl, dc);
	}
	ctop += step;
	clamptops();
	return 0;
}

/* Put the compose cursor at the cell under content pixel (px,py). */
static
setcurxy(px, py)
{
	int r, dcw;

	r = (py - by0) / cellh;
	if ( r < 0 )
		r = 0;
	dotl = ctop + r;
	if ( dotl > nln - 1 ) dotl = nln - 1;
	if ( dotl < 0 ) dotl = 0;
	dcw = offx + (px - xpix) / cellw;
	if ( dcw < 0 )
		dcw = 0;
	dotc = byteat(dotl, dcw);
	return 0;
}

/* Append the PRIMARY selection to header field f (middle-click on a field):
 * printable characters only, stopping at the first newline -- an address
 * list or a subject is one line by definition. */
static
hdrpaste(f)
{
	char b[64];
	register char *p;
	long off, len;
	int n, i, c, room, plen;

	p = (f == 0) ? hto : hsub;
	room = fldroom(f == 0 ? tofw : sjfw);
	len = hr_sellen();
	if ( len <= 0 )
		return 0;
	for ( off = 0; off < len; off += n )
	{
		n = hr_selread(off, b, sizeof(b));
		if ( n <= 0 )
			break;
		for ( i = 0; i < n; i++ )
		{
			c = b[i] & 0xff;
			if ( c == '\n' )
				return 0;
			plen = strlen(p);
			if ( c >= ' ' && c < 0x7f &&
			     plen < sizeof(hto) - 1 && (plen + 1) * fcw < room )
			{
				p[plen] = c;
				p[plen + 1] = 0;
				edited = 1;
			}
		}
	}
	return 0;
}

/* Insert the PRIMARY selection at the compose cursor (middle-click). */
static
inssel()
{
	char b[64];
	long off, len;
	int n, i, c;

	len = hr_sellen();
	if ( len <= 0 )
		return 0;
	for ( off = 0; off < len; off += n )
	{
		n = hr_selread(off, b, sizeof(b));
		if ( n <= 0 )
			break;
		for ( i = 0; i < n; i++ )
		{
			c = b[i] & 0xff;
			if ( c == '\n' )
				donl();
			else if ( c == '\t' || (c >= ' ' && c < 0x7f) )
				inschar(c);
		}
	}
	fixview();
	return 0;
}

/* ------------------------------------------------------------------ */
/* commands (the buttons)                                             */
/* ------------------------------------------------------------------ */

/* Start a composition: to/subj prefill the header FIELDS (either may be "");
 * the body buffer starts empty.  foc says where the caret goes -- New wants
 * the To: field so an address can be typed at once, Reply has both fields
 * filled and wants the body. */
static
startcompose(to, subj, foc)
char *to, *subj;
{
	freebuf();
	newbuf();
	if ( nln == 0 )
		return 0;
	sprintf(hto, "%.60s", to);
	sprintf(hsub, "%.60s", subj);
	hdrfoc = foc;
	composing = 1;
	edited = 0;
	ctop = 0;
	offx = 0;
	dotl = 0;
	dotc = 0;
	layout();			/* the header strip changes the body rows */
	invalidate();
	loadseek = -1;
	return 0;
}

static
endcompose()
{
	composing = 0;
	edited = 0;
	hdrfoc = 2;
	layout();
	invalidate();
	viewmsg(selmsg);
	return 0;
}

static
donew()
{
	if ( composing )
		return 0;
	startcompose("", "", 0);
	return 0;
}

static
doreply()
{
	char sb[NSUBJ + 4];

	if ( composing || selmsg < 0 )
		return 0;
	if ( strncmp(msubj[selmsg], "Re: ", 4) == 0 )
		strcpy(sb, msubj[selmsg]);
	else
		sprintf(sb, "Re: %.35s", msubj[selmsg]);
	startcompose(mfrom[selmsg], sb, 2);
	return 0;
}

static
dodelete()
{
	if ( composing || selmsg < 0 )
		return 0;
	mdel[selmsg] = !mdel[selmsg];
	chromedirty = 1;		/* the deleted count changed */
	return 0;
}

/* Copy [start,end) of the still-open old spool stream onto nfp. */
static
mcopy(ifp, ofp, start, end)
FILE *ifp, *ofp;
long start, end;
{
	register int c;

	fseek(ifp, start, 0);
	while ( start++ < end && (c = getc(ifp)) != EOF )
		putc(c, ofp);
	return 0;
}

/* Rewrite the spool without the deleted messages (7mail's mquit): the open
 * mfp still reads the OLD inode after the unlink, so the undeleted bytes are
 * copied from it into the fresh file, then everything is rescanned. */
static
dopurge()
{
	register int i;
	FILE *nfp;
	struct stat sb;

	if ( composing || ndeleted() == 0 )
		return 0;
	if ( !confirm("Remove the deleted messages?") )
		return 0;
	if ( mfp == 0 || fstat(fileno(mfp), &sb) < 0 )
		return 0;
	mlock(myuid);
	unlink(spoolname);
	if ( (nfp = fopen(spoolname, "w")) == 0 )
	{
		munlock();
		rescan();
		notice("Cannot rewrite the mailbox");
		return 0;
	}
	chown(spoolname, sb.st_uid, sb.st_gid);
	chmod(spoolname, sb.st_mode & 0777);
	for ( i = 0; i < nmsg; i++ )
		if ( !mdel[i] )
			mcopy(mfp, nfp, mseek[i], mend[i]);
	fclose(nfp);
	munlock();
	sync();
	/* The marks are consumed by the rewrite -- clear them BEFORE the
	 * rescan, or its keep-marks-by-offset carryover re-marks whatever
	 * message shifted down into a purged one's old byte position. */
	for ( i = 0; i < nmsg; i++ )
		mdel[i] = 0;
	rescan();
	viewmsg(selmsg);
	invalidate();
	return 0;
}

static
dosend()
{
	char *names[NADDR];
	int nu;

	if ( !composing )
		return 0;
	nu = parseto(names, NADDR);
	if ( nu <= 0 )
	{
		notice("No recipient in the To: field");
		return 0;
	}
	if ( deliver(names, nu, 0) < 0 )
	{
		notice(dmsg);
		return 0;
	}
	endcompose();
	mailflag = 1;			/* mail to self shows up at once */
	return 0;
}

static
docancel()
{
	if ( !composing )
		return 0;
	if ( edited && !confirm("Discard this message?") )
		return 0;
	endcompose();
	return 0;
}

static
dobutton(i)
{
	if ( composing )
	{
		if ( i == 0 )
			dosend();
		else
			docancel();
		return 0;
	}
	switch ( i )
	{
	case 0:	donew();	break;
	case 1:	doreply();	break;
	case 2:	dodelete();	break;
	case 3:	dopurge();	break;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

/* A keystroke for the focused header field: append/rub out at the end (the
 * dialog DW_TEXT editing model); Tab, Enter and ^N/down move on to the next
 * field and then the body, ^P/up goes back up.  The caret placement is
 * hdrfoc's job -- this only edits and moves the focus. */
static
hdrkey(c)
{
	register char *p;
	int len, room;

	hdrdirty = 1;
	if ( c == '\t' || c == '\r' || c == '\n' || c == 'N'-0x40 )
	{
		hdrfoc++;			/* To -> Subject -> body */
		return 0;
	}
	if ( c == 'P'-0x40 )
	{
		if ( hdrfoc > 0 )
			hdrfoc--;
		return 0;
	}
	p = (hdrfoc == 0) ? hto : hsub;
	len = strlen(p);
	if ( c == '\b' || c == 0x7f )
	{
		if ( len > 0 )
		{
			p[len - 1] = 0;
			edited = 1;
		}
		return 0;
	}
	room = fldroom(hdrfoc == 0 ? tofw : sjfw);
	if ( c >= ' ' && c < 0x7f &&
	     len < sizeof(hto) - 1 && (len + 1) * fcw < room )
	{
		p[len] = c;
		p[len + 1] = 0;
		edited = 1;
	}
	return 0;
}

static
dokey(c)
{
	c &= 0xff;
	if ( composing )
	{
		if ( hdrfoc < 2 )
		{
			hdrkey(c);
			return 0;
		}
		switch ( c )
		{
		case '\r':
		case '\n':	donl();			break;
		case '\b':	backspc();		break;
		case 0x7f:
		case 'D'-0x40:	delfwd();		break;
		case 'A'-0x40:	dotc = 0;		break;
		case 'E'-0x40:	dotc = strlen(ln[dotl]);	break;
		case 'B'-0x40:	mvleft();		break;
		case 'F'-0x40:	mvright();		break;
		case 'P'-0x40:				/* up from the body's top */
			if ( dotl == 0 && dotc == 0 )	/* line goes to the fields */
			{
				hdrfoc = 1;
				hdrdirty = 1;
			}
			else
				mvup();
			break;
		case 'N'-0x40:	mvdown();		break;
		case 'Z'-0x40:	pgmove(-1);		break;
		case 'V'-0x40:	pgmove(1);		break;
		default:
			if ( c == '\t' || (c >= ' ' && c < 0x7f) )
				inschar(c);
		}
		fixview();
		return 0;
	}
	switch ( c )
	{
	case 'P'-0x40:	select(selmsg - 1);	break;
	case 'N'-0x40:	select(selmsg + 1);	break;
	case 'Z'-0x40:
	case 'B'-0x40:	pgmove(-1);		break;
	case 'V'-0x40:
	case 'F'-0x40:
	case ' ':	pgmove(1);		break;
	case 'd':	dodelete();		break;
	case 'n':	donew();		break;
	case 'r':	doreply();		break;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

tick()
{
	mailflag = 1;
	signal(SIGALRM, tick);
	alarm(15);
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int need, i;
	struct stat sb;

	if ( setname() < 0 )
		exit(1);
	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	fcw = hr_font(SHM_FUI)->cellw;
	fch = hr_font(SHM_FUI)->cellh;
	if ( fcw <= 0 ) fcw = 9;
	if ( fch <= 0 ) fch = 16;
	xpix = ((HRSB_W + cellw - 1) / cellw) * cellw;
	me.ha_w = xpix + 80 * cellw;
	me.ha_h = BARH + 2 + 8 * cellh + 3 + 25 * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	layout();

	newbuf();
	if ( nln == 0 )
	{
		hr_bye();
		exit(1);
	}
	rescan();
	if ( selmsg >= 0 )
	{
		if ( selmsg < ltop || selmsg >= ltop + lrows )
			ltop = selmsg - lrows + 1;
		viewmsg(selmsg);
	}

	invalidate();
	need = 1;			/* flushed below, or by the first loop
					 * pass if a server overlay is up now */
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		flush();
		need = 0;
	}

	signal(SIGALRM, tick);
	alarm(15);
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
				invalidate();
				need = 1;
				break;

			case E_RESIZE:
				contw = e.wm_arg[0];
				conth = e.wm_arg[1];
				layout();
				clamptops();
				invalidate();
				need = 1;
				break;

			case E_KEY:
				dokey(e.wm_arg[0]);
				need = 1;
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					if ( (i = btnhit(e.wm_arg[0],
							 e.wm_arg[1])) >= 0 )
					{
						barmed = i;
						barin = 1;
						cl_begin();
						cl_fillrect(bx[i] + 1, BTNY + 1,
						    bx[i] + bw[i] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= ly0 &&
						  e.wm_arg[1] < ly0 + lrows * cellh )
					{
						if ( hr_sbpress(&sbl, e.wm_arg[1]) )
						{
							ltop = sbl.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= by0 )
					{
						if ( hr_sbpress(&sbc, e.wm_arg[1]) )
						{
							ctop = sbc.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[1] >= ly0 &&
						  e.wm_arg[1] < ly0 + lrows * cellh )
					{
						if ( !composing )
						{
							i = ltop + (e.wm_arg[1] - ly0)
								   / cellh;
							if ( i >= 0 && i < nmsg )
							{
								select(i);
								need = 1;
							}
						}
					}
					else if ( composing &&
						  e.wm_arg[1] >= cy0 &&
						  e.wm_arg[1] < by0 )
					{	/* the header strip: focus the field hit */
						i = e.wm_arg[0];
						if ( i >= tofx && i < tofx + tofw )
							hdrfoc = 0;
						else if ( i >= sjfx && i < sjfx + sjfw )
							hdrfoc = 1;
						hdrdirty = 1;
						need = 1;
					}
					else if ( e.wm_arg[1] >= by0 && composing )
					{
						hdrfoc = 2;
						hdrdirty = 1;
						setcurxy(e.wm_arg[0], e.wm_arg[1]);
						fixview();
						need = 1;
					}
				}
				else				/* release */
				{
					if ( barmed >= 0 )
					{
						i = barmed;
						barmed = -1;
						if ( barin )
						{
							cl_begin();
							cl_fillrect(bx[i] + 1,
							    BTNY + 1,
							    bx[i] + bw[i] - 1,
							    BTNY + DLG_BTNH - 1, 2);
							cl_end();
							dobutton(i);
							need = 1;
						}
					}
					else if ( sbl.sb_drag )
						hr_sbrelease(&sbl);
					else if ( sbc.sb_drag )
						hr_sbrelease(&sbc);
				}
				break;

			case E_MOTION:
				if ( barmed >= 0 )
				{
					i = (btnhit(e.wm_arg[0], e.wm_arg[1])
					     == barmed);
					if ( i != barin )
					{
						barin = i;
						cl_begin();
						cl_fillrect(bx[barmed] + 1,
						    BTNY + 1,
						    bx[barmed] + bw[barmed] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
				}
				else if ( sbl.sb_drag )
				{
					if ( hr_sbmotion(&sbl, e.wm_arg[1]) )
					{
						ltop = sbl.sb_pos;
						need = 1;
					}
				}
				else if ( sbc.sb_drag )
				{
					if ( hr_sbmotion(&sbc, e.wm_arg[1]) )
					{
						ctop = sbc.sb_pos;
						need = 1;
					}
				}
				break;

			case E_PASTE:		/* middle-click: insert PRIMARY */
				if ( composing && e.wm_arg[1] >= cy0 &&
				     e.wm_arg[1] < by0 )
				{	/* into the header field under the pointer */
					i = e.wm_arg[0];
					if ( i >= tofx && i < tofx + tofw )
						hdrfoc = 0;
					else if ( i >= sjfx && i < sjfx + sjfw )
						hdrfoc = 1;
					if ( hdrfoc < 2 )
						hdrpaste(hdrfoc);
					hdrdirty = 1;
					need = 1;
				}
				else if ( composing && e.wm_arg[1] >= by0 )
				{
					hdrfoc = 2;
					hdrdirty = 1;
					setcurxy(e.wm_arg[0], e.wm_arg[1]);
					inssel();
					need = 1;
				}
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )		/* fell behind: assume the worst */
		{
			invalidate();
			need = 1;
		}
		if ( mailflag )			/* poll the spool for new mail */
		{
			mailflag = 0;
			i = 0;
			if ( stat(spoolname, &sb) < 0 )
			{
				if ( nmsg > 0 )
					i = 1;
			}
			else if ( sb.st_size != boxsize )
				i = 1;
			if ( i && !composing )
			{
				rescan();
				viewmsg(selmsg);
				invalidate();
				need = 1;
			}
		}
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )	/* a draw was lost against a freeze */
			{
				invalidate();
				need = 1;
			}
			if ( need )
			{
				flush();
				need = 0;
			}
		}
	}
}
