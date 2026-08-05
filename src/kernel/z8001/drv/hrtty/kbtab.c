/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore 900 Keyboard driver
 * Extended (more function keys, etc)
 * IBM PC compatible keyboard.
 * Tables to drive the state machine.
 */

#include <kbchar.h>
#include <kbtab.h>

/*
 * This table is indexed by the scan codes.
 * The comments contain scan codes (in hex) and key
 * numbers (from the Commodore Keyboard Documentation,
 * in decimal).
 */
KEY	ktab[] =	{
	KINV,	0,	0,	0,		/* Unused */
	0,	CESC,	CESC,	0,		/* SC01, K12 */
	0,	'1',	'!',	0,		/* SC02, K13 */
	KC,	'2',	'@',	0,		/* SC03, K14 */
	0,	'3',	'#',	0,		/* SC04, K15 */
	0,	'4',	'$',	0,		/* SC05, K16 */
	0,	'5',	'%',	0,		/* SC06, K17 */
	0,	'6',	'^',	0,		/* SC07, K18 */
	0,	'7',	'&',	0,		/* SC08, K19 */
	0,	'8',	'*',	0,		/* SC09, K20 */
	0,	'9',	'(',	0,		/* SC0A, K21 */
	0,	'0',	')',	0,		/* SC0B, K22 */
	0,	'-',	'_', 	0,		/* SC0C, K23 */
	0,	'=',	'+',	0,		/* SC0D, K24 */
	0,	'\b',	'\b',	0,		/* SC0E, K26 */
	0,	'\t',	'\t',	0,		/* SC0F, K27 */
   KCAP|KC,	'q',	'Q',	ct('q'),	/* SC10, K28 */
   KCAP|KC,	'w',	'W',	ct('w'),	/* SC11, K29 */
   KCAP|KC,	'e',	'E',	ct('e'),	/* SC12, K30 */
   KCAP|KC,	'r',	'R',	ct('r'),	/* SC13, K31 */
   KCAP|KC,	't',	'T',	ct('t'),	/* SC14, K32 */
   KCAP|KC,	'y',	'Y',	ct('y'),	/* SC15, K33 */
   KCAP|KC,	'u',	'U',	ct('u'),	/* SC16, K34 */
   KCAP|KC,	'i',	'I',	ct('i'),	/* SC17, K35 */
   KCAP|KC,	'o',	'O',	ct('o'),	/* SC18, K36 */
   KCAP|KC,	'p',	'P',	ct('p'),	/* SC19, K37 */
	KC,	'[',	'{',	CESC,		/* SC1A, K38 */
	KC,	']',	'}',	035,		/* SC1B, K39 */
	KC,	'\r',	'\r',	'\r',		/* SC1C, K40 */
	KSHIFT,	0,	0,	SCT,		/* SC1D, K42 CTRL */
   KCAP|KC,	'a',	'A',	ct('a'),	/* SC1E, K43 */
   KCAP|KC,	's',	'S',	ct('s'),	/* SC1F, K44 */
   KCAP|KC,	'd',	'D',	ct('d'),	/* SC20, K45 */
   KCAP|KC,	'f',	'F',	ct('f'),	/* SC21, K46 */
   KCAP|KC,	'g',	'G',	ct('g'),	/* SC22, K47 */
   KCAP|KC,	'h',	'H',	ct('h'),	/* SC23, K48 */
   KCAP|KC,	'j',	'J',	ct('j'),	/* SC24, K49 */
   KCAP|KC,	'k',	'K',	ct('k'),	/* SC25, K50 */
   KCAP|KC,	'l',	'L',	ct('l'),	/* SC26, K51 */
	0,	';',	':',	0,		/* SC27, K52 */
	0,	'\'',	'"',	0,		/* SC28, K53 */
	KC,	'`',	'~',	ct('`'),	/* SC29, K54 */
	KSHIFT,	0,	0,	SS1,		/* SC2A, K55 - Left Shift */
	KC,	'\\',	'|',	ct('|'),	/* SC2B, K25 */
   KCAP|KC,	'z',	'Z',	ct('z'),	/* SC2C, K56 */
   KCAP|KC,	'x',	'X',	ct('x'),	/* SC2D, K57 */
   KCAP|KC,	'c',	'C',	ct('c'),	/* SC2E, K58 */
   KCAP|KC,	'v',	'V',	ct('v'),	/* SC2F, K59 */
   KCAP|KC,	'b',	'B',	ct('b'),	/* SC30, K60 */
   KCAP|KC,	'n',	'N',	ct('n'),	/* SC31, K61 */
   KCAP|KC,	'm',	'M',	ct('m'),	/* SC32, K62 */
	0,	',',	'<',	0,		/* SC33, K63 */
	0,	'.',	'>',	0,		/* SC34, K64 */
	0,	'/',	'?',	0,		/* SC35, K65 */
	KSHIFT,	0,	0,	SS2,		/* SC36, K66 - Right Shift */
	KP,	'*',	'*',	PF3,		/* SC37, K76 */
	KSHIFT,	0,	0,	SAL,		/* SC38, K67 - Alt Key */
	0,	' ',	' ',	0,		/* SC39, K68 */
	KSHIFT, 0,	0,	SCL,		/* SC3A, K69 - CAPS Lock */
	KC,	F1,	F1,	F1,		/* SC3B, K1 */
	KC,	F2,	F2,	F2,		/* SC3C, K2 */
	KC,	F3,	F3,	F3,		/* SC3D, K3 */
	KC,	F4,	F4,	F4,		/* SC3E, K4 */
	KC,	F5,	F5,	F5,		/* SC3F, K5 */
	KC,	F6,	F6,	F6,		/* SC40, K6 */
	KC,	F7,	F7,	F7,		/* SC41, K7 */
	KC,	F8,	F8,	F8,		/* SC42, K8 */
	KC,	F9,	F9,	F9,		/* SC43, K9 */
	KC,	F10,	F10,	F10,		/* SC44, K10 */
	KC,	F14,	F14,	F14,		/* SC45, K72 */
	KC,	F15,	F15,	F15,		/* SC46, K73 */
	KP,	'7',	'7',	K7,		/* SC47, K81 */
	KP,	'8',	'8',	K8,		/* SC48, K82 */
	KP,	'9',	'9',	K9,		/* SC49, K83 */
	KP,	'-',	'-',	KMINUS,		/* SC4A, K84 */
	KP,	'4',	'4',	K4,		/* SC4B, K85 */
	KP,	'5',	'5',	K5,		/* SC4C, K86 */
	KP,	'6',	'6',	K6,		/* SC4D, K87 */
	KP,	'+',	'+',	KPLUS,		/* SC4E, K88 */
	KP,	'1',	'1',	K1,		/* SC4F, K89 */
	KP,	'2',	'2',	K2,		/* SC50, K90 */
	KP,	'3',	'3',	K3,		/* SC51, K91 */
	KP,	'0',	'0',	K0,		/* SC52, K93 */
   KDUP|KP,	'0',	'0',	K00,		/* SC53, K94 */
	KC,	F11,	F11,	F11,		/* SC54, K11 - HELP */
	KC,	CDEL,	CDEL,	037,		/* SC55, K41 */
	KC,	F12,	F12,	F12,		/* SC56, K70 */
	KC,	F13,	F13,	F13,		/* SC57, K71 */
	KP,	CCE,	CCE,	PF1,		/* SC58, K74 */
	KP,	'.',	'.',	PF2,		/* SC59, K75 */
	KP,	'/',	'/',	PF4,		/* SC5A, K77 */
	KC,	W1,	W1,	W1,		/* SC5B, K78 */
	KC,	W2,	W2,	W2,		/* SC5C, K79 */
	KC,	W3,	W3,	W3,		/* SC5D, K80 */
	KP,	'\r',	'\r',	KENTER,		/* SC5E, K92 */
	0,	CUP,	CUP,	0,		/* SC5F, K95 */
	0,	CLEFT,	CLEFT,	0,		/* SC60, K96 */
	0,	CRIGHT,	CRIGHT,	0,		/* SC61, K97 */
	0,	CDOWN,	CDOWN,	0,		/* SC62, K98 */
	KC,	O1,	O1,	O1,		/* SC63, K99 */
	KC,	O2,	O2,	O2		/* SC64, K100 */
};

/*
 * Extent of the table above.  A raw scan code from the hardware runs to
 * 0x7F, well past the last entry, so the interrupt handler has to bound
 * check before indexing; keeping the count here means it stays right when
 * the table grows.
 */
int	nktab = sizeof ktab / sizeof ktab[0];
