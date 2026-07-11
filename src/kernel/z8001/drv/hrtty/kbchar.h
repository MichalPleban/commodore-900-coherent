/*
 * Commodore 900 Keyboard driver.
 * Character definitions from the keyboard.
 * Eventually, all of this will be replaced
 * by an ANSI keyboard definition
 */
/* Special keys */
#define	CUP	0x80		/* Cursor UP */
#define	CLEFT	0x81		/* Cursor LEFT */
#define	CRIGHT	0x82		/* Cursor RIGHT */
#define	CDOWN	0x83		/* Cursor DOWN */
#define	CE	0x84		/* Clear entry */

#define	CSLOCK	0x8A		/* Scroll lock */
#define	CBRK	0x8B		/* Break key */

/* Shift states */
#define	CSHIFT	0x90		/* Start of shift state characters */
#define	CSHEND	0x97		/* End of shift state characters */
#define	isshift(c)	(((c)&0x07)==CSHIFT)
#define	SUPPER	0x01		/* Bit for upper case shifted */
#define	SCTRL	0x02		/* Bit for control shift */
#define	SALT	0x04		/* Bit for ALT shift */

/* Function keys */
#define	FBASE	F1		/* Base of function keys */
#define	F1	0xC0
#define	F2	0xC1
#define	F3	0xC2
#define	F4	0xC3
#define	F5	0xC4
#define	F6	0xC5
#define	F7	0xC6
#define	F8	0xC7
#define	F9	0xC8
#define	F10	0xC9
#define	F11	0xCA
#define	F12	0xCB
#define	F13	0xCC
#define	F14	0xCD
#define	F15	0xCE
#define	W1	0xCF			/* Mouse function keys */
#define	W2	0xD0
#define	W3	0xD1
#define	PF1	0xD2			/* CE - start of keypad keys */
#define	PF2	0xD3			/* . */
#define	PF3	0xD4			/* * */
#define	PF4	0xD5			/* / */
#define	K0	0xD6			/* Keypad mode 0 */
#define	K1	0xD7
#define	K2	0xD8
#define	K3	0xD9
#define	K4	0xDA
#define	K5	0xDB
#define	K6	0xDC
#define	K7	0xDD
#define	K8	0xDE
#define	K9	0xDF
#define	K00	0xE0
#define	KENTER	0xE1			/* Enter key */
#define	KPLUS	0xE2			/* + key */
#define	KMINUS	0xE3			/* - key */
#define	O1	0xE4			/* Optional key #1 */
#define	O2	0xE5			/* Optional key #2 */
#define	FEND	O2			/* End of function keys */
