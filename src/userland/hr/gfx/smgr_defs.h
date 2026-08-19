/*
 *		General system defines that we like 
 */
#ifndef NULL
#define NULL (0)
#endif
#define BOOLEAN	int

/*
 * 	Constant value defines  
 */
/*
 *	Display dimensions 
 */
#define	DIS_HEIGHT	(YMAX - YMIN)
#define DIS_WIDTH	(XMAX - XMIN)

/*
 *	BIT-BLT Logical operations: src logop dst
 */
#define L_FALSE 0	/* all 0's		*/
#define L_AND	1	/* src & dst		*/
#define L_ANDN	2	/* src & ~dst		*/
#define L_SRC	3	/* src			*/
#define L_NAND	4	/* (~src) & dst		*/
#define L_DST	5	/* dst			*/
#define L_XOR	6	/* src ^ dst		*/
#define L_OR	7	/* src | dst		*/
#define L_NANDN	8	/* (~src) & (~dst)	*/
#define L_NXOR	9	/* (~src) ^ dst		*/
#define L_NDST	10	/* ~dst			*/
#define L_ORN	11	/* src | (~dst)		*/
#define L_NSRC	12	/* ~src			*/
#define L_NOR	13	/* (~src) | dst		*/
#define L_NORN	14	/* (~src) | (~dst)      */
#define	L_TRUE	15	/* all 1's		*/
#define L_CHAR	16	/* exchange src with dst*/
#define L_BLTMAX 16	/* highest blt logical op*/




/* 
 *	Window type defines for screen manager 
 */
#define WT_SMARTUPDATE 0x01
#define WT_NOOBS       0x02
#define WT_NORELOC     0x04
#define WT_NOSIZE      0x08
#define WT_OUTPUT      0x10
#define WT_NODECOR     0x20	/* undecorated window (wire.h HRF_NODECOR):
				 * outline() draws nothing for it -- no frame,
				 * no title bar, no drop shadow -- and the
				 * content rect is the whole layer rect */

#define WT_FULLY_VIS	0x01
#define WT_CURSOR_ON	0x02
#define WT_CURSOR_WON	0x04
#define WT_2_CURSOR_WON	0x08

/* Event mask fields */
#define KBD_ENABLE	0x0001
#define WMK_ENABLE	0x0002
#define APK_ENABLE	0x0004
#define DEF_EVMASK	KBD_ENABLE	

#define	SM_LFT		0x8000
#define SM_MID		0x4000
#define SM_RGHT		0x2000

#define SM_DTK		(SM_RGHT)
#define SM_WMK		(SM_MID)
#define SM_APK		(SM_LFT)

/*
 *	Constants for internal screen manager 
 */
/* 16 is the ceiling the shared VRAM tail allows without relaying it out: the
 * SHM_SURF clip table is MAX_WINDOWS * sizeof(HRSURF) (~110 B) from 0x3100 and
 * must stay below the lock word at 0x3800 -- 16 * 110 = 0x6E0, ending 0x37E0. */
#define MAX_WINDOWS	(16)
#define MAX_RBUF	(100)
#define MAX_UPBUF	(50)
#define L_EMPTY		(0)
#define L_VISIBLE	(1)
#define L_OBSCURED	(2)
#define NO_WINDOW     	(-1)

/* Window decoration metrics (GUI.md 2.7).  The drop shadow lives INSIDE the
 * layer rect (right+bottom WD_SHADOW px) so it clips and clears with the window;
 * WD_TITLEH is the title-bar strip at the top; WD_BORDER is the frame padding
 * on the remaining sides.  Shared by the engine (outline) and the server. */
#define WD_SHADOW	6
#define WD_TITLEH	20
#define WD_BORDER	2



/*
 *	Constants
 */

#define	MAXPEN	(32)		/* maximum pen width or height		*/
#define MVERTEX	(255)		/* maximum number of vertices		*/
#define	MRADIUS (512)		/* maximum radius			*/
#define MDIAMETER (1024+32)	/* maximum rectangular dimension	*/

#define MAXCOLOR	1	/* maximum color number			*/
#define	BLACK		0	/*	black color			*/
#define	WHITE		1	/*	white color			*/

#define	MAXPAT		10	/* maximum pattern number		*/
#define FP_BACK		0	/* 	background color		*/
#define FP_FORE		1	/*	foreground color		*/
#define FP_GREY		2	/*	light halftone			*/
#define	FP_ARROW	3	/*	arrow				*/
#define FP_HALF		4	/*	halftone			*/
#define FP_VBAR		5	/*	vertical bars			*/
#define FP_STRIPE2	6	/* 	double stripe			*/
#define FP_DIAG		7	/*	hatch pattern			*/
#define	FP_CBM		8	/*	cbm chicken			*/
#define	FP_BRICK	9	/*	brick wall			*/


#define	BadVerb		(-1)	/*	illegal action verb		*/
#define	VB_FRAME	1	/*	frame the shape			*/
#define	VB_FILL		2	/*	fill with the foreground pattern*/
#define	VB_PAINT	4	/*	fill with the given pattern	*/
#define	VB_ERASE	6	/*	fill with the background pattern*/
#define	VB_INVERT	8	/*	fill with the inverted dest	*/


/*
 *	Font Styles
 */
#define	NTXSTYLES	(5)		/* number of style variations	*/
#define	TXBOLD		0x01		/* bold			*/
#define	TXULINE		0x02		/* underlined		*/
#define	TXOLINE		0x04		/* outlined		*/
#define TXSHADOW	0x08		/* shadowed		*/
#define	TXITALIC	0x10		/* italic		*/


/*
 *	Graphics State Control Values
 */
#define GCPEN       (0x0001)       /* Pen parameters         */
#define GCLOG       (0x0002)       /* Logop                  */
#define GCFPAT      (0x0004)       /* Foreground pattern     */
#define GCBPAT      (0x0008)       /* Background pattern     */
#define GCFCOLOR    (0x0010)       /* Foreground color       */
#define GCBCOLOR    (0x0020)       /* Background color       */
#define GCFONT      (0x0040)       /* Font parameters        */
#define GCPAT       (0x0080)       /* Definable fill pattern */
#define GCALL       (0x00ff)       /* all graphics values    */

#define	SET_ADD	(0)
#define SET_AND	(1)
#define SET_SUB (2)
#define SET_XOR (3)

