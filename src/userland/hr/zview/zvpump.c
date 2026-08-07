/*
 * zvpump.c - zview's input pump as a TINY separate program.
 *
 * The pump used to run as a plain fork of the server (the V7 two-process
 * split, GUI.md sec 4.5/7: the pump blocks in CIOGETM while the server
 * blocks in read(), and neither needs select()).  But zview is linked as
 * ONE ~69 Kb software segment (no shared text, no separated I/D), so that
 * fork held a full contiguous copy of the whole server image for the life
 * of the desktop -- the same "fork clones the whole data/BSS" cost that
 * made zterm's pumps a separate program (hrpump.c).  This is the same cure:
 * zvpump links libc only (a few Kb), and startpump() (zview.c) execs it.
 *
 * It inherits the write end of the server's command pipe on HR_CMDFD
 * (startpump dup2's it) and forwards every keyboard/mouse event from the
 * driver as a C_INPUT record.  The driver draws the arrow cursor itself.
 */
#include "smgr.h"
#include "wire.h"

/* the driver's default arrow cursor sprite (from the old smgr; zview keeps
 * its own copy for restoring the arrow after menu/drag cursors). */
static int DEF_MOUSE[] = { 0xfffc, 0xfff8, 0xfff0, 0xffe0,
			   0xffc0, 0xffc0, 0xffe0, 0xfff0,
			   0xfff8, 0xfffc, 0xf3fe, 0xe1ff,
			   0x80ff, 0x007f, 0x003e, 0x001c };

/* Scancode -> ASCII, ported verbatim from the historical hi-res keyboard
 * message layer (kev.c SM_Keyboard, GUI.md 5.3), so the server gets real
 * ASCII.  Moved here from zview.c with the pump itself.
 *
 * ONE departure from the original tables: the cursor/nav block (the XT keypad
 * positions 0x47..0x53, which the original dropped -- it never tracked
 * numlock) now emits the MicroEMACS control codes:
 *     Up ^P  Down ^N  Left ^B  Right ^F   Home ^A  End ^E
 *     PgUp ^Z  PgDn ^V   keypad Del -> DEL
 * The wire stays plain ASCII, so nothing downstream changes: an editor that
 * already binds the MicroEMACS set gets working arrows for free, a shell in a
 * zterm sees them as the control keys a user could have typed anyway (and
 * MicroEMACS running INSIDE a terminal window gets exactly the codes it
 * wants), and dialogs ignore them.  The keypad digits never worked here
 * (numlock was never handled), so nothing is lost.
 *
 * The FUNCTION keys deliver the HRK_* codes of wire.h (above ASCII):
 * F1-F10 at the XT positions 0x3B..0x44, and the C900 specials as F11-F15.
 * Which scancodes the specials use on REAL hardware is only partly known:
 * Help is 0x54 (the hr driver's own Alt+Ctrl+Help hatch tests that code,
 * and the historical table has the C900's DEL right beside it at 0x55);
 * for Clear/Home, Pop/Push, Screen/Print and Stop/Continue we take the
 * gfx/kbd.h block 0x5A..0x5D (that header is otherwise unused, but it is
 * the one place those keys are named), plus 0x5F as an alternate Help.
 * The emulator front ends send exactly these, so under emulation all five
 * work; on the real machine F1-F10 and Help are certain, the rest are a
 * best guess in table slots that were dead anyway. */
#define KB_KEYUP	0x80
#define KB_KEYSC	0x7f
#define KB_LSHIFT	(0x2a-1)
#define KB_RSHIFT	(0x36-1)
#define KB_CTRL		(0x1d-1)
#define KB_ALT		(0x38-1)
#define KB_CAPLOCK	(0x3a-1)
#define KB_SRS	0x01
#define KB_SLS	0x02
#define KB_CTS	0x04
#define KB_ALS	0x08
#define KB_CPLS	0x10
#define KB_NMLS	0x20
#define KB_SHFT	0x80
#define KB_SES	(KB_SLS|KB_SRS)
#define KB_SS1	(KB_SLS|KB_SRS|KB_CTS)
#define KB_LET	(KB_SLS|KB_SRS|KB_CPLS|KB_CTS)
#define XXX	0377
#define SPC	0376
#define DEL	0x7f

static unsigned char lmaptab[] ={
	     '\33',  '1',  '2',  '3',  '4',  '5',  '6',
	 '7',  '8',  '9',  '0',  '-',  '=', '\b', '\t',
	 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
	 'o',  'p',  '[',  ']', '\r',  XXX,  'a',  's',
	 'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
	 '\'', '`',  XXX,  '\\',  'z',  'x',  'c',  'v',
	 'b',  'n',  'm',  ',',  '.',  '/',  XXX,  SPC,
	 XXX,  ' ',  XXX, '\201','\202','\203','\204','\205',
	'\206','\207','\210','\211','\212', SPC,  SPC, '\001',
	'\020','\032', '-', '\002', SPC, '\006', '+', '\005',
	'\016','\026', SPC,  DEL, '\213', DEL,  SPC,  SPC,
	 SPC,  SPC, '\214','\215','\216','\217', '\r','\213',
	 SPC,  SPC,  SPC,  XXX,  XXX
};
static unsigned char umaptab[] ={
	     '\33',  '!',  '@',  '#',  '$',  '%',  '^',
	 '&',  '*',  '(',  ')',  '_',  '+', '\b', '\t',
	 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
	 'O',  'P',  '{',  '}', '\r',  XXX,  'A',  'S',
	 'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
	 '"',  '~',  XXX,  '|',  'Z',  'X',  'C',  'V',
	 'B',  'N',  'M',  '<',  '>',  '?',  XXX,  SPC,
	 XXX,  ' ',  XXX, '\201','\202','\203','\204','\205',
	'\206','\207','\210','\211','\212', SPC,  SPC, '\001',
	'\020','\032', '-', '\002', SPC, '\006', '+', '\005',
	'\016','\026', SPC,  DEL, '\213', DEL,  SPC,  SPC,
	 SPC,  SPC, '\214','\215','\216','\217', '\r','\213',
	 SPC,  SPC,  SPC,  XXX,  XXX
};
#define SS0	0
#define SS1	(KB_SLS|KB_SRS|KB_CTS)
#define SES	(KB_SLS|KB_SRS)
#define LET	(KB_SLS|KB_SRS|KB_CPLS|KB_CTS)
#define KEY	(KB_SLS|KB_SRS|KB_NMLS|0x40)
#define SHFT	KB_SHFT
static unsigned char smaptab[] ={
	       SS0,  SES,  SS1,  SES,  SES,  SES,  SS1,
	 SES,  SES,  SES,  SES,  SS1,  SES,  SS0,  SS0,
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  LET,
	 LET,  LET,  SS1,  SS1,  SS0, SHFT,  LET,  LET,
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  SES,
	 SES,  SS1, SHFT,  SS1,  LET,  LET,  LET,  LET,
	 LET,  LET,  LET,  SES,  SES,  SES, SHFT,  SS0,
	SHFT,  SS1, SHFT,  SS0,  SS0,  SS0,  SS0,  SS0,
	 SS0,  SS0,  SS0,  SS0,  SS0,  SS0,  KEY,  KEY,
	 KEY,  KEY,  SS0,  KEY,  KEY,  KEY,  SS0,  KEY,
	 KEY,  KEY,  KEY,  KEY,  SS0,  SS0,  SS0,  SS0,
	 SS0,  SS0,  SS0,  SS0,  SS0,  SS0,  SS0,  SS0,
	 SS0,  SS0,  SS0,  LET,  LET
};
#undef SHFT

static int kbshift = 0;

/* Translate a raw scancode to ASCII; return -1 for releases, modifiers and
 * dead/special keys (which the server ignores). */
static
keymap(r)
int r;
{
	register int c, s;

	r &= 0xff;
	if ( r == 0xff )
		return -1;
	c = (r & KB_KEYSC) - 1;
	if ( c < 0 || c >= sizeof(smaptab) )
		return -1;
	s = smaptab[c];
	if ( s & KB_SHFT )
	{
		if ( r & KB_KEYUP )
		{
			if ( c == KB_RSHIFT ) kbshift &= ~KB_SRS;
			else if ( c == KB_LSHIFT ) kbshift &= ~KB_SLS;
			else if ( c == KB_CTRL ) kbshift &= ~KB_CTS;
			else if ( c == KB_ALT ) kbshift &= ~KB_ALS;
		}
		else
		{
			if ( c == KB_LSHIFT ) kbshift |= KB_SLS;
			else if ( c == KB_RSHIFT ) kbshift |= KB_SRS;
			else if ( c == KB_CTRL ) kbshift |= KB_CTS;
			else if ( c == KB_ALT ) kbshift |= KB_ALS;
			else if ( c == KB_CAPLOCK ) kbshift ^= KB_CPLS;
		}
		return -1;
	}
	if ( r & KB_KEYUP )
		return -1;
	if ( kbshift & KB_CTS )
	{
		if ( s == KB_SS1 || s == KB_LET )
			c = umaptab[c] & 0x1f;
		else
			return -1;
	}
	else if ( s &= kbshift )
	{
		if ( kbshift & KB_SES )
			c = (s & (KB_CPLS|KB_NMLS)) ? lmaptab[c] : umaptab[c];
		else
			c = (s & (KB_CPLS|KB_NMLS)) ? umaptab[c] : lmaptab[c];
	}
	else
		c = lmaptab[c];
	if ( c == XXX || c == SPC )
		return -1;
	return c & 0xff;
}

main()
{
	int fd;
	MESSAGE m;
	WMSG c;

	fd = open("/dev/smgr", 2);
	if ( fd < 0 )
		_exit(1);
	ioctl(fd, CIOEVMGR);
	ioctl(fd, CIOMOUSE, DEF_MOUSE);
	ioctl(fd, CIOMSEON, (char *)0);

	c.wm_wid = 0;
	c.wm_type = C_INPUT;
	for (;;)
	{
		if ( ioctl(fd, CIOGETM, &m) < 0 )
			continue;
		if ( m.msg_Cmd == SM_MOUSE )
		{
			c.wm_arg[0] = IN_MOVE;
			c.wm_arg[1] = m.msg_Data[1] & 0x1fff;
			c.wm_arg[2] = m.msg_Data[2] & 0x1fff;
		}
		else if ( m.msg_Cmd == SM_MKEY )
		{
			c.wm_arg[0] = IN_BUTTON;
			c.wm_arg[1] = m.msg_Data[1] & 0x1fff;	/* x            */
			c.wm_arg[2] = m.msg_Data[2] & 0x1fff;	/* y            */
			c.wm_arg[3] = m.msg_Data[2] & 0xe000;	/* buttons down */
			c.wm_arg[4] = m.msg_Data[1] & 0xe000;	/* changed bits */
		}
		else if ( m.msg_Cmd == SM_KKEY )
		{
			int a = keymap(m.msg_Data[1]);
			if ( a < 0 )
				continue;	/* release / modifier / dead key */
			c.wm_arg[0] = IN_KEY;
			c.wm_arg[1] = a;
		}
		else
			continue;
		write(HR_CMDFD, &c, sizeof(c));
	}
}
