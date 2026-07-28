#define MAXKDN  10      /* # of down keys */
#define MAXREP 	10	/* repeat count */
 
/* modifier flags */
#define MSHFT	0x01	/* shift modifier bit */
#define MLOCK	0x02	/* caps lock modifier bit */
#define	MCTRL	0x04	/* control key modifier bit */
#define MALT1	0x08	/* alternate1 mod bit */
#define MALT2	0x10	/* alternate2 mod bit */
#define MRIGH	0x20	/* kbd mouse button right mod bit */
#define MMIDD	0x40    /* kbd mouse button mid   mod bit */
#define MLEFT   0x80    /* kbd mouse button left  mod bit */
#define BRIGH   0x01    /* mech mouse button right    */
#define BMIDD   0x02    /* mech mouse button middle   */
#define BLEFT   0x04    /* mech mouse button left     */
 
#define SHFT_KEY 0x61
#define CAPS_KEY 0x62
#define CTRL_KEY 0x63
#define ALT1_KEY 0x64
#define ALT2_KEY 0x64


/* cursor arrow keys         */
#define KCUP     0x4C /* UP   */
#define KCDWN    0x4D /* DOWN */
#define KCLFT    0x4F /* LEFT */
#define KCRGHT   0x4E /* RIGHT*/

/* Misc. other keys          */
#define CLR      0x5A 
#define HOME     0x5A
#define POP      0x5B
#define PUSH	 0x5B
#define SPRIN	 0x5C
#define SSTOP    0x5D
#define SCONT    0x5D
#define CLEAR	 0x5E
#define HELP     0x5F

#define BS  0x08
#define TAB 0x09
#define DEL 0x7F
#define RET 0x0D
#define ESCP 0x1B
#define SPACE 0x20
#define SQUOTE 0x27  /* '  */
#define	BQUOTE 0x60  /* `  */
#define DQUOTE 0x22  /* "  */
#define SLASH  0x2F  /* /  */
#define BSLASH 0x5C  /* \ */
#define NOPE 0x0
#define PHLD NOPE    /* place holder */ 

typedef struct  kdn{
	char	dn_keypos;
	char	dn_keyval;
	int	dn_cnt;
	}KDN;
