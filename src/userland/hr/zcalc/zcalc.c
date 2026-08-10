/*
 * zcalc.c - a ZView desk calculator.
 *
 * A fixed-size window: a display box over a 4x5 grid of buttons drawn with
 * the shared button chrome (zprint's), pressed with the mouse or driven from
 * the keyboard.  Immediate-execution arithmetic, the way desk calculators
 * work: an operator key applies the PENDING operator to the accumulator and
 * the display, shows the result, and remembers the new operator; "=" applies
 * and forgets.  The display string IS the current value (atof of it is the
 * operand), so what you see is exactly what is calculated with.
 *
 * Buttons:   sin cos  C  CE  +/-  /    keys: c=C, e=CE, +/-/x/*, digits, '.',
 *            tan ln   7   8   9   x         '=' or Enter, BS/DEL rubs out a
 *            log e^x  4   5   6   -         digit while entering; s=sin,
 *            sqrt y^x 1   2   3   +         o=cos, t=tan, l=ln, g=log,
 *            pi  1/x  0   .   =             j=e^x, r=sqrt, ^=y^x, p=pi, i=1/x
 * The pending operator is shown at the left edge of the display.
 *
 * The scientific keys are the usual immediate ones: a unary function (sin,
 * cos, tan, ln, log, e^x, sqrt, 1/x) applies to the DISPLAY value on the
 * spot, pi types the constant, and y^x is an operator like the four basics.
 * Trig is in radians.  Domain errors (sqrt of a negative, log of a
 * non-positive, 1/0) show "Error" like division by zero.
 *
 * Arithmetic is double precision (the real dtoa formatter is linked, so
 * "%.10g" works); division by zero or overflow shows "Error" and locks
 * everything but C.
 */
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"

extern double	atof();

/* Layout, px (UI font is 9x16). */
#define	MARG	8		/* window margin                          */
#define	DSPY	8		/* display box top                        */
#define	DSPH	24		/* display box height                     */
#define	GRIDY	(DSPY + DSPH + 10)
#define	BTNW	44		/* button size and grid gaps              */
#define	BTNH	26
#define	GAPX	8
#define	GAPY	7

#define	NDISP	14		/* display text width, chars             */

/* The button grid: label and the key it acts as.  '=' spans two cells. */
struct btn {
	char	*label;
	int	key;		/* the dokey() character                 */
	int	col, row;
	int	span;		/* grid cells wide                       */
};
struct btn btns[] = {
	{ "sin",  's', 0, 0, 1 }, { "cos", 'o', 1, 0, 1 },
	{ "C",    'c', 2, 0, 1 }, { "CE",  'e', 3, 0, 1 },
	{ "+/-",  'n', 4, 0, 1 }, { "/",   '/', 5, 0, 1 },
	{ "tan",  't', 0, 1, 1 }, { "ln",  'l', 1, 1, 1 },
	{ "7",    '7', 2, 1, 1 }, { "8",   '8', 3, 1, 1 },
	{ "9",    '9', 4, 1, 1 }, { "x",   '*', 5, 1, 1 },
	{ "log",  'g', 0, 2, 1 }, { "e^x", 'j', 1, 2, 1 },
	{ "4",    '4', 2, 2, 1 }, { "5",   '5', 3, 2, 1 },
	{ "6",    '6', 4, 2, 1 }, { "-",   '-', 5, 2, 1 },
	{ "sqrt", 'r', 0, 3, 1 }, { "y^x", '^', 1, 3, 1 },
	{ "1",    '1', 2, 3, 1 }, { "2",   '2', 3, 3, 1 },
	{ "3",    '3', 4, 3, 1 }, { "+",   '+', 5, 3, 1 },
	{ "pi",   'p', 0, 4, 1 }, { "1/x", 'i', 1, 4, 1 },
	{ "0",    '0', 2, 4, 1 }, { ".",   '.', 3, 4, 1 },
	{ "=",    '=', 4, 4, 2 },
};
#define	NBTN	(sizeof(btns) / sizeof(btns[0]))

HRAPP	me = { "Calculator", "calc.icn", 0, 0, 0, 0, 0, 0 };

int	mywid;
int	fcw, fch;		/* UI-font cell                           */
int	contw, conth;		/* granted content size, px               */

/* ---- calculator state ---- */
char	disp[24];		/* the display string = the current value */
double	acc;			/* the accumulator                        */
int	pendop;			/* pending operator char, 0 = none        */
int	entering;		/* 1 = disp is being typed               */
int	errstate;		/* 1 = "Error": only C works              */

int	armed = -1;		/* pressed button, -1 = none              */
int	armin;			/* 1 = pointer currently inside it        */

/* ------------------------------------------------------------------ */
/* geometry                                                           */
/* ------------------------------------------------------------------ */

static
btnx(b)
struct btn *b;
{
	return MARG + b->col * (BTNW + GAPX);
}

static
btny(b)
struct btn *b;
{
	return GRIDY + b->row * (BTNH + GAPY);
}

static
btnw(b)
struct btn *b;
{
	return b->span * BTNW + (b->span - 1) * GAPX;
}

/* Which button is under content pixel (x,y)?  -1 = none. */
static
btnhit(x, y)
{
	register int i;
	register struct btn *b;

	for ( i = 0; i < NBTN; i++ )
	{
		b = &btns[i];
		if ( x >= btnx(b) && x < btnx(b) + btnw(b)
		  && y >= btny(b) && y < btny(b) + BTNH )
			return i;
	}
	return -1;
}

/* ------------------------------------------------------------------ */
/* drawing                                                            */
/* ------------------------------------------------------------------ */

/* One button: white face, 1px border, 2px drop shadow, centred label
 * (the 9x16 UI glyphs sit 1px high-left in their cell, so centre +1,+1). */
static
drawbtn(i)
{
	register struct btn *b;
	int x, y, w, tx, ty;

	b = &btns[i];
	x = btnx(b);  y = btny(b);  w = btnw(b);
	cl_fillrect(x, y, x + w, y + BTNH, 1);
	cl_fillrect(x, y, x + w, y + 1, 0);
	cl_fillrect(x, y + BTNH - 1, x + w, y + BTNH, 0);
	cl_fillrect(x, y, x + 1, y + BTNH, 0);
	cl_fillrect(x + w - 1, y, x + w, y + BTNH, 0);
	cl_fillrect(x + 2, y + BTNH, x + w + 2, y + BTNH + 2, 0);
	cl_fillrect(x + w, y + 2, x + w + 2, y + BTNH + 2, 0);
	tx = x + (w - strlen(b->label) * fcw) / 2 + 1;
	ty = y + (BTNH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, tx, ty, b->label);
	if ( i == armed && armin )
		cl_fillrect(x + 1, y + 1, x + w - 1, y + BTNH - 1, 2);
	return 0;
}

/* The display: 1px box, the pending operator at the left edge, the value
 * right-aligned. */
static
drawdisp()
{
	char t[4];
	int x0, x1, tx, ty, n;

	x0 = MARG;
	x1 = contw - MARG;
	cl_fillrect(x0, DSPY, x1, DSPY + DSPH, 1);
	cl_fillrect(x0, DSPY, x1, DSPY + 1, 0);
	cl_fillrect(x0, DSPY + DSPH - 1, x1, DSPY + DSPH, 0);
	cl_fillrect(x0, DSPY, x0 + 1, DSPY + DSPH, 0);
	cl_fillrect(x1 - 1, DSPY, x1, DSPY + DSPH, 0);
	ty = DSPY + (DSPH - fch) / 2 + 1;
	if ( pendop )
	{
		t[0] = pendop == '*' ? 'x' : pendop;
		t[1] = 0;
		cl_ptext(SHM_FUI, x0 + 5, ty, t);
	}
	n = strlen(disp);
	tx = x1 - 5 - n * fcw + 1;
	cl_ptext(SHM_FUI, tx, ty, disp);
	return 0;
}

static
drawall()
{
	register int i;

	cl_fillrect(0, 0, contw, conth, 1);
	drawdisp();
	for ( i = 0; i < NBTN; i++ )
		drawbtn(i);
	return 0;
}

/* ------------------------------------------------------------------ */
/* the calculator                                                     */
/* ------------------------------------------------------------------ */

static
seterr()
{
	strcpy(disp, "Error");
	errstate = 1;
	pendop = 0;
	entering = 0;
	return 0;
}

/* Show a result: it becomes the display string, and so the next operand. */
static
show(v)
double v;
{
	if ( v > 1.0e99 || v < -1.0e99 )
		return seterr();
	sprintf(disp, "%.10g", v);
	if ( strlen(disp) > NDISP )	/* pathological formatting */
		sprintf(disp, "%.6g", v);
	return 0;
}

static double
calc(a, op, b)
double a, b;
{
	switch ( op )
	{
	case '+':	return a + b;
	case '-':	return a - b;
	case '*':	return a * b;
	case '/':
		if ( b == 0.0 )
		{
			errstate = 1;
			return 0.0;
		}
		return a / b;
	case '^':
		errno = 0;
		a = pow(a, b);
		if ( errno )
		{
			errstate = 1;
			return 0.0;
		}
		return a;
	}
	return b;
}

/* A unary scientific key: apply the function to the display value on the
 * spot; the result becomes the display (and so the next operand). */
static
unary(c)
{
	double v;

	v = atof(disp);
	switch ( c )
	{
	case 's':	v = sin(v);	break;
	case 'o':	v = cos(v);	break;
	case 't':	v = tan(v);	break;
	case 'l':
		if ( v <= 0.0 )
			return seterr();
		v = log(v);
		break;
	case 'g':
		if ( v <= 0.0 )
			return seterr();
		v = log10(v);
		break;
	case 'j':	v = exp(v);	break;
	case 'r':
		if ( v < 0.0 )
			return seterr();
		v = sqrt(v);
		break;
	case 'i':
		if ( v == 0.0 )
			return seterr();
		v = 1.0 / v;
		break;
	}
	entering = 0;
	return show(v);
}

static
digit(c)
{
	register int n;

	if ( !entering )
	{
		strcpy(disp, "");
		entering = 1;
	}
	n = strlen(disp);
	if ( c == '.' )
	{
		register int i;

		for ( i = 0; disp[i]; i++ )
			if ( disp[i] == '.' )
				return 0;
		if ( n == 0 )
		{
			strcpy(disp, "0.");
			return 0;
		}
	}
	else if ( n == 1 && disp[0] == '0' )
	{
		disp[0] = c;
		return 0;
	}
	if ( n >= NDISP - 1 )
		return 0;
	disp[n] = c;
	disp[n + 1] = 0;
	return 0;
}

static
negate()
{
	char t[24];

	if ( disp[0] == '-' )
		strcpy(t, disp + 1);
	else if ( strcmp(disp, "0") != 0 )
	{
		t[0] = '-';
		strcpy(t + 1, disp);
	}
	else
		return 0;
	strcpy(disp, t);
	return 0;
}

static
operator(c)
{
	double v;

	if ( pendop && !entering )
	{
		pendop = c;		/* just change your mind */
		return 0;
	}
	v = atof(disp);
	if ( pendop )
	{
		acc = calc(acc, pendop, v);
		if ( errstate )
			return seterr();
		show(acc);
	}
	else
		acc = v;
	pendop = c;
	entering = 0;
	return 0;
}

static
equals()
{
	if ( pendop == 0 )
		return 0;
	acc = calc(acc, pendop, atof(disp));
	pendop = 0;
	entering = 0;
	if ( errstate )
		return seterr();
	show(acc);
	return 0;
}

static
clearall()
{
	strcpy(disp, "0");
	acc = 0.0;
	pendop = 0;
	entering = 0;
	errstate = 0;
	return 0;
}

static
rubout()
{
	register int n;

	if ( !entering )
		return 0;
	n = strlen(disp);
	if ( n > 0 )
		disp[n - 1] = 0;
	if ( disp[0] == 0 || strcmp(disp, "-") == 0 )
	{
		strcpy(disp, "0");
		entering = 0;
	}
	return 0;
}

/* One key (from the keyboard or a button); returns 1 when the display needs
 * repainting. */
static
dokey(c)
{
	c &= 0xff;
	if ( c == 'c' || c == 'C' )
	{
		clearall();
		return 1;
	}
	if ( errstate )
		return 0;
	switch ( c )
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	case '.':
		digit(c);
		return 1;

	case '+': case '-': case '/': case '^':
		operator(c);
		return 1;

	case 's': case 'o': case 't': case 'l':
	case 'g': case 'j': case 'r': case 'i':
		unary(c);
		return 1;

	case 'p': case 'P':
		show(3.14159265358979);
		entering = 0;
		return 1;

	case '*': case 'x': case 'X':
		operator('*');
		return 1;

	case '=': case '\r': case '\n':
		equals();
		return 1;

	case 'n': case 'N':
		negate();
		return 1;

	case 'e': case 'E':
		strcpy(disp, "0");	/* CE: clear the entry */
		entering = 0;
		return 1;

	case 0x08: case 0x7f:
		rubout();
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

main(argc, argv)
char **argv;
{
	WMSG e;
	int need, wasidle, i;

	fcw = hr_font(SHM_FUI)->cellw;
	fch = hr_font(SHM_FUI)->cellh;
	if ( fcw <= 0 ) fcw = 9;
	if ( fch <= 0 ) fch = 16;
	me.ha_w = 2 * MARG + 6 * BTNW + 5 * GAPX + 2;
	me.ha_h = GRIDY + 5 * BTNH + 4 * GAPY + 2 + MARG;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;

	clearall();

	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		cl_begin();
		drawall();
		cl_end();
	}

	need = 0;
	wasidle = 0;
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
			case E_RESIZE:
				need = 1;
				break;

			case E_KEY:
				if ( dokey(e.wm_arg[0]) )
				{
					cl_begin();
					drawdisp();
					cl_end();
				}
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					if ( (i = btnhit(e.wm_arg[0],
							 e.wm_arg[1])) >= 0 )
					{
						armed = i;
						armin = 1;
						cl_begin();
						drawbtn(i);
						cl_end();
					}
				}
				else				/* release */
				{
					if ( armed >= 0 )
					{
						i = armed;
						armed = -1;
						cl_begin();
						drawbtn(i);
						if ( armin &&
						     dokey(btns[i].key) )
							drawdisp();
						cl_end();
						armin = 0;
					}
				}
				break;

			case E_MOTION:
				if ( armed >= 0 )
				{
					i = (btnhit(e.wm_arg[0], e.wm_arg[1])
					     == armed);
					if ( i != armin )
					{
						armin = i;
						cl_begin();
						drawbtn(armed);
						cl_end();
					}
				}
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )
			need = 1;
		cl_refresh();
		if ( cl_frozen() || !cl_mapped() )
			wasidle = 1;
		else
		{
			if ( wasidle )
			{
				need = 1;
				wasidle = 0;
			}
			if ( need )
			{
				cl_begin();
				drawall();
				cl_end();
				need = 0;
			}
		}
	}
}
