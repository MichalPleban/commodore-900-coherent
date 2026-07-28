
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
#define	CIOACK		('c'<<8 | 10)
#define	CIOEVMGR	('c'<<8 | 11)
#define CIOUEVMGR	('c'<<8 | 12)
#define	CIOHOLD		('c'<<8 | 13)
#define	CIOUHOLD	('c'<<8 | 14)
#define	CIOWAIT		('c'<<8 | 15)
#define	CIODUMP		('c'<<8 | 16)
#define CIOFLUSH	('c'<<8 | 17)



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
#define	WMGR		2		/* window managers (4 of them) */
#define	WINDOW		6		/* windows (14 of them) */
#define	DESTMAX		20		/* first invalid src/dst */
#define	INDIRECT	0100		/* indirect-destination bit */


#define NARGS (3)

typedef struct {
	int	msg_Sender;
	int 	msg_Cmd;
	int 	msg_Data[NARGS];
	} MESSAGE;
#define msg_Receiver msg_Sender



typedef struct xfer {
	uint	x_src,
		x_count;
	char	*x_srcp,
		*x_dstp;
	} XFER ;


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
