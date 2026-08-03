

#define	NARG	4			/* # words in a message */

/*
 * command argument to ioctl( )
 */
#define	CIOSENDM	('c'<<8 | 0)
#define	CIOGETM		('c'<<8 | 1)
#define	CIOSIG		('c'<<8 | 2)
#define	CIOMOUSE	('c'<<8 | 3)
#define	CIOMSEON        ('c'<<8 | 4)
#define CIOMSEOFF       ('c'<<8 | 5)
#define	CIOGETD		('c'<<8 | 6)
#define	CIODATA		('c'<<8 | 7)
#define	CIOSGTTY	('c'<<8 | 8)
#define	CIOTCHRS	('c'<<8 | 9)
#define CIOACK		('c'<<8 | 10)
#define	CIOEVMGR	('c'<<8 | 11)
#define CIOUEVMGR	('c'<<8 | 12)
#define	CIOHOLD		('c'<<8 | 13)
#define	CIOUHOLD	('c'<<8 | 14)
#define CIOWAIT		('c'<<8 | 15)
#define	CIODUMP		('c'<<8 | 16)	/* no longer implemented	*/
#define CIOFLUSH	('c'<<8 | 17)
#define	CIOCRSON	('c'<<8 | 18)
#define	CIOCRSOFF	('c'<<8 | 19)
#define	CIOMLOCK	('c'<<8 | 20)	/* block until the drawing lock is free, take it */
#define	CIOMUNLOCK	('c'<<8 | 21)	/* wake processes blocked on the drawing lock    */
#define	CIOEVWAIT	('c'<<8 | 22)	/* sleep until event ring <arg> is non-empty     */
#define	CIOEVWAKE	('c'<<8 | 23)	/* wake whoever sleeps on event ring <arg>       */



/*
 * screen manager
 *	Header info for messages from driver.
 *	m_msg[0]
 */
#define	SM_KKEY		(0)		/* keyboard input */
#define	SM_MKEY		(1)		/* mouse key input */
#define	SM_MOUSE	(2)		/* mouse whereabouts report */



/*
 *  window managers
 *	Header info for messages from driver.
 *	m_msg[0]
 */
#define	WM_OPEN		(0)		/* open a window	*/
#define	WM_CLOSE	(1)		/* close a window	*/
#define	WM_GETC		(2)		/* char wanted by appl. */
#define	WM_PUTD		(3)		/* chars from appl. to be written */
#define	WM_TIOGETP 	(4)		/* sgttyb report	*/
#define	WM_TIOSETP 	(5)		/* set sgttyb		*/
#define	WM_TIOSETN	(6)		/* set sgttyb, no flush	*/
#define WM_TIOGETC 	(7)		/* tchars report	*/
#define	WM_TIOSETC 	(8)		/* set tchars		*/
#define	WM_TIOFLSH	(9)		/* flush		*/


/*
 * message sources/destinations
 *	The order may not be changed.  The parameters may not be changed,
 * except WINDOW and DESTMAX.  When sending messages, the INDIRECT bit directs
 * delivery to the window mgr of the dst rather than the dst itself.
 */
#define	DRIVER		0		/* hr console driver */
#define	SMGR		1		/* screen manager */
#define	DMGR		2		/* desktop manager	*/
#define	WMGR		2		/* window managers (4 of them) */
#define	WINDOW		6		/* windows (14 of them) */
#define	DESTMAX		20		/* first invalid src/dst */
#define	INDIRECT	0100		/* indirect-destination bit */


struct message {
	int	m_src,
		m_msg[NARG];
};
#define	m_dst	m_src


struct xfer {
	uint	x_src,
		x_count;
	char	*x_srcp,
		*x_dstp;
};


/*
 * screen hardware stuff
 */
#define XMIN	(0)
#define YMIN	(0)
#define	XMAX	(1024)
#define	YMAX	(800)
#define	YSPLIT	(512)
#define	SEG0	((uint *) 0x3a000000L)
#define	SEG1	((uint *) 0x3b000000L)


/*
 * mouse hardware stuff
 * We assume x and y position data to be in the low MBITS of the words from
 * XPORT and YPORT.  Mouse key states (up/down) are found in bits DSMENU,
 * DWMENU and DACTION of XPORT.
 */
#define	XPORT	0x400			/* relative-x bits & key states */
#define	YPORT	0x402			/* relative-y bits & key states */
#define	MBITS	10			/* # bits of mouse position data */
#define	DSMENU	0x8000			/* screen menu key */
#define	DWMENU	0x4000			/* window menu key */
#define	DACTION	0x2000			/* action key */
