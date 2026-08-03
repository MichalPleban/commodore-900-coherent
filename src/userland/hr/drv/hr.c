

/*
 * Commodore Z8000-HR console
 */
#include	<coherent.h>
#include	<drvcon.h>	/* was <con.h>: the device-switch CON/DRV header
				   was renamed drvcon.h in the current kernel tree */
#include	<io.h>
#include	<uproc.h>
#include	<errno.h>
#include	<sched.h>
#include	<sgtty.h>
#include	<signal.h>
#include	<stat.h>
#include	<rico.h>
#include	"hr.h"


/*
** ZCIO1 base address
*/
#define	ZCIO1	0x0000
#define	PKBD	0x0205	/* keyboard enable port		*/
#define	KBDEN	0x02	/* keyboard enable bit		*/
#define PC2	0x04
#define	PC3	0x08

/*
 * Control register addresses
 */
#define	MICR	0x01	/* master interrupt		*/
#define	MCCR	0x03	/* master configuration		*/

/*
 * Register addresses for Port A
 */
#define	PACAS	0x11	/* port a command and status	*/
#define	PAMS	0x41	/* port a mode specification	*/
#define	PAHS	0x43	/* port a handshake spec	*/
#define	PADPP	0x45	/* port a data path polarity	*/
#define	PADD	0x47	/* port a data direction	*/
#define	PASIOC	0x49	/* port a special io control	*/
#define	PAPP	0x4b	/* port a pattern palarity	*/
#define	PAPT	0x4d	/* port a pattern transition	*/
#define	PAPM	0x4f	/* port a patterm mask		*/
#define	PAIV	0x05	/* port a interrupt vector	*/
#define	PADATA	0x1b	/* port a data			*/

/*
 * Register addresses for Port C
 */
#define	PCDD	0x0d	/* port c data direction	*/
#define	PCSIOC	0x0f	/* port c special io control	*/
#define	PCDPP	0x0b	/* port c data path polarity	*/
#define	PCDATA	0x1f	/* port c data			*/

/*
 * Misc hardware flags and values
 */
#define	MIE	0x80	/* master interrupt enable	*/
#define	PAE	0x04	/* port a enable		*/
#define	C_IPIUS 0x20	/* clear IP&IUS command		*/

#define	IRF	0x04	/* input register full		*/
#define	HRVECTOR 8	/* port a interrupt vector	*/


#define	MAJOR	7		/* major device # of the console */
#define	NMBUF	100		/* # message buffers */
#define	MDELAY	10		/* delay (ticks) between mouse pos reports */
#define	NOEVMGR	(-1)		/* indicates no event manager set */
#define	MOUSEWI	16		/* mouse cursor bit width	*/
#define	MOUSEHI	16		/* mouse cursor bit height	*/


/* flags
 */
#define	WRITING	0x0001			/* appl. is blocked in hrwrite	*/
#define	READING	0x0002			/* appl. is blocked in hrread	*/
#define	WANTMB	0x0004			/* mbuf wanted			*/
#define NEEDDAT 0x0008			/* appl. waiting for simple data*/
#define	GETDAT	0x0010			/* appl. blocked in getting data*/
#define	NEEDMSG	0x0020			/* device blocked for message	*/
#define	EXCL	0x0040			/* device is exclusive use	*/
#define	BUSY	0x0080			/* previous send failed	(EDBUSY)*/


/*
 * Index into client's sleeping array for event types
 */
#define	SLP_WRIT	(0)
#define	SLP_READ	(1)
#define	SLP_MBUF	(2)
#define	SLP_NDAT	(3)
#define	SLP_GETD	(4)
#define	SLP_MSG		(5)
#define	SLP_MAX		(5)


struct cdata {
	int		i;
	struct sgttyb	tty;
};


struct client {
	uint		flags,
			wtype,		/* window manager (WINDOW only) */
			pid,		/* window managers only		*/
			group;		/* process group */
	int		ocount;		/* open count	*/
	struct mbuf	*head,		/* messages waiting to be read */
			*tail;		/* last message in queue */
	char		*sleeping[SLP_MAX+1];	/* event awaited, many types */
	int		datc;		/* read character data */
	struct cdata	data;		/* misc client data */
};

struct mbuf {
	struct mbuf	*mb_next;
	struct message	mb_m;
};


/* states of the xfer mechanism
 */
#define	XIDLE	0
#define	XFILL	1
#define	XDRAIN	2
#define	XERROR	3


/* event manager list
 */
struct evmgr {
	uint		edev;		/* device	*/
	int		emsen;
	struct evmgr	*enext;
};


/* mouse cursor buffer
 */
struct mouse
{
	ulong	ms_buf[16]; 		/* mouse pattern buffer		*/
	int	ms_x,			/* last x position drawn	*/
		ms_y,			/* last y position drawn	*/
		ms_bit,			/* bit alignment of pat in buf	*/
		ms_en;			/* cursor drawing enabled flag	*/
};

struct mouse mousebuf =
{
	{
		0xffff0000L, 0xffff0000L, 0xffff0000L, 0xffff0000L,
		0xffff0000L, 0xffff0000L, 0xffff0000L, 0xffff0000L,
		0xffff0000L, 0xffff0000L, 0xffff0000L, 0xffff0000L,
		0xffff0000L, 0xffff0000L, 0xffff0000L, 0xffff0000L
	},
	0,
	0,
	7,
	0
};


uint		xstate,			/* state of the xfer mechanism */
		xbufn;			/* # chars waiting in `xbuf' */
char		xbuf[100];		/* kernel buffer for xfer mechanism */
struct mbuf	mbuf[NMBUF],		/* message buffers */
		*mbufp,			/* head of the free message buffers */
		*mshead,		/* head of mouse button msg list */
		*mstail;		/* end of mouse button msg list */
struct client	client[DESTMAX];	/* message clients */
struct evmgr	evmgr[WINDOW],		/* room for all managers */
		*evmfree,		/* free evmgr list	*/
		*evmlist;		/* in-use evmgr entry	*/
struct xfer	x;			/* user args for xfer operation */
uint		hrsmgr,			/* who is event mgr	*/
		hrislocked;		/* is SMGR blocked? */
		hrticks;		/* time indicator for mouse and keys */
struct message	mouse;			/* mouse position report for SMGR */
struct message	tiomsg;			/* misc TIO function data	*/
TIM		timebuf;		/* to call hrmouse( ) every tick */


saddr_t		kbpamap;		/* original kbd OS segment mapping */
int		(*kbpaivect)();		/* original keyboard ivector routine */
uint		kbpaiv;			/* original keyboard ivector */
uint		kbpamccr;		/* original keyboard MCCR register */


int	hropen( ),
	hrclose( ),
	hrread( ),
	hrwrite( ),
	hrioctl( ),
	hrload( ),
	hruload( ),
	nulldev();

struct mbuf *hralloc();

CON hrcon = {
	DFCHR,
	MAJOR,
	hropen,
	hrclose,
	nulldev,
	hrread,
	hrwrite,
	hrioctl,
	nulldev,
	nulldev,
	hrload,
	hruload
};


hrload( )
{
	register struct mbuf	*p;
	register struct client	*cp;
	register struct evmgr	*ev;
	int	s;
	extern	void	hrkey();
	extern	int	(*vecs[])();
	extern	saddr_t	vmaps[];

	/*
	 *	Initialize clients, free lists, etc.
	 */
	hrsmgr = NOEVMGR;
	mbufp = mbuf;
	for (p=mbuf; p<endof( mbuf)-1; ++p)
		p->mb_next = p + 1;
	for (cp=client; cp< &client[WINDOW + 8]; ++cp)
		cp->wtype = WMGR + 1;
	for ( ; cp < &client[WINDOW + 10] ; ++cp )
		cp->wtype = WMGR + 3;
	for (; cp< endof(client); ++cp)
		cp->wtype = WMGR + 2;
	for ( ev=evmgr ; ev<endof( evmgr)-1; ev++)
		ev->enext = ev + 1;
	evmfree = evmgr;
	evmlist = NULL;
	mouse.m_src = DRIVER;
	mouse.m_msg[0] = not SM_MOUSE;

	/*
	 *	Old Keyboard Status
	 */
	kbpaiv = inb(ZCIO1+PAIV);
	kbpamccr = inb(ZCIO1+MCCR);
	kbpaivect = vecs[kbpaiv >> 1];
	kbpamap = vmaps[kbpaiv >> 1];

	/*
	 *	Keyboard Load
	 */
	s = sphi();
	if ( kbpamccr & PAE )
		clrivec(kbpaiv);
	setivec(HRVECTOR, hrkey);
	outb(ZCIO1+PAMS,  0x1a);
	outb(ZCIO1+PAHS,  0x00);
	outb(ZCIO1+PADPP, 0x00);
	outb(ZCIO1+PADD,  0xff);
	outb(ZCIO1+PASIOC,0x00);
	outb(ZCIO1+PAPP,  0x80);
	outb(ZCIO1+PAPT,  0x00);
	outb(ZCIO1+PAPM,  0x80);
	outb(ZCIO1+PAIV,  HRVECTOR);
	outb(ZCIO1+PACAS, 0xc0);
	outb(ZCIO1+MICR, inb(ZCIO1+MICR)|MIE);
	outb(ZCIO1+MCCR, inb(ZCIO1+MCCR)|PAE);
	outb(PKBD, KBDEN);
	outb(ZCIO1+PCDATA, 0);		/* toggle pc3	*/
	outb(ZCIO1+PCDATA, PC3);
	spl(s);
}


hruload()
{
	int	s;
	extern	saddr_t	vmaps[];

	mousebuf.ms_en = 0;
	timeout( &timebuf, 0, NULL, 0);
	s = sphi();
	outb(ZCIO1+MCCR, kbpamccr);
	clrivec(HRVECTOR);
	if ( kbpamccr & PAE )
	{
		setivec(kbpaiv, kbpaivect);
		vmaps[kbpaiv >> 1] = kbpamap;
	}
	spl(s);
}


hropen( dev, mode)
dev_t	dev;
{
	register struct client	*cp;
	register PROC	*pp;
	register uint	d;
	struct	 mbuf	*p;

	d = minor( dev);
	if (d >= DESTMAX) {
		u.u_error = ENXIO;
		return;
	}
	cp = &client[d];
	pp = SELF;
	if ( cp->flags & EXCL ) {
		u.u_error = EDBUSY;
		return;
	}
	if ( d<WINDOW )
	{
		/* DMGR is the shared cursor-control handle: the window server AND
		 * every direct-render client (Model A) open it concurrently to
		 * bracket their own blits with CIOMSEON/OFF, so it must NOT be
		 * exclusive and must never become anyone's controlling tty.  Every
		 * other low minor stays single-open (exclusive).  Cross-process
		 * hide/show composes because the driver ref-counts the hide depth
		 * (see hrmseon/hrmseoff in hr2.c). */
		if ( d != DMGR )
		{
			cp->flags = EXCL;
			if (pp->p_ttdev == NODEV)
				pp->p_ttdev = dev;
		}
		if ( cp->ocount == 0 )
		{
			cp->pid = pp->p_pid;
			cp->group = pp->p_group;
		}
		cp->ocount++;
#ifdef DEBUG
printf("HROPEN: dev %d, pid %d, group %d\n", d, pp->p_pid, pp->p_group);
#endif
		return;
	}
	else
	{
		if ( cp->ocount )
			goto setgroup;
		hrlock(d, NEEDDAT);
		if ( cp->ocount )
		{
			cp->flags &= ~NEEDDAT;
			goto setgroup;
		}
		p = hralloc(d);
		p->mb_m.m_src = d;
		p->mb_m.m_msg[0] = WM_OPEN;
		hrlink(cp->wtype, p);
		if ( hrwait(d, SLP_NDAT) == FALSE )
			return;
		if ( cp->data.i )
		{
			u.u_error = ENXIO;
			return;
		}
   setgroup:
		cp->ocount++;
		if ( pp->p_group == client[DMGR].group ) {
			if ( not cp->group )
				cp->group = pp->p_pid;
			pp->p_group = cp->group;
		}
		if (pp->p_ttdev == NODEV)
			pp->p_ttdev = dev;
#ifdef DEBUG
printf("HROPEN: dev %d, pid %d, pgroup %d, cgroup %d\n",
d, pp->p_pid, pp->p_group, cp->group);
#endif
	}
}


hrclose( dev)
dev_t	dev;
{
	register struct client	*cp;
	register struct mbuf	*p;
	uint	self;

	self = minor( dev);
	cp = &client[self];
	if ( --cp->ocount == 0 )
	{
		if ( self >= WINDOW )
		{
			hrlock(self, NEEDDAT);
			p = hralloc( self );
			p->mb_m.m_src = self;
			p->mb_m.m_msg[0] = WM_CLOSE;
			hrlink(cp->wtype, p);
			hrwait(self, SLP_NDAT);
		}
		else
			hruevmgr(self);
		cp->flags = 0;
		cp->pid = 0;
		cp->group = 0;
	}
}


hrread( dev, iop)
dev_t	dev;
IO	*iop;
{
	register struct mbuf	*p;
	register struct client	*cp;
	register uint		self;

	if (not iop->io_ioc)
		return;
	self = minor( dev);
	cp = &client[self];
	hrlock(self, READING);
	p = hralloc(self);
	p->mb_m.m_src = self;
	p->mb_m.m_msg[0] = WM_GETC;
	hrlink(cp->wtype, p);
	if ( hrwait(self, SLP_READ) == FALSE )
		return;
	if ( cp->datc >= 0 )
		ioputc( cp->datc, iop);
}


hrwrite( dev, iop)
dev_t		dev;
register IO	*iop;
{
	register struct mbuf	*p;
	register struct client	*cp;
	register uint		self;
	bool	xcheck( );

	if (not iop->io_ioc)
		return;
	self = minor( dev);
	cp = &client[self];
	hrlock(self, WRITING);
	p = hralloc(self);
	p->mb_m.m_src = self;
	p->mb_m.m_msg[0] = WM_PUTD;
	p->mb_m.m_msg[1] = iop->io_ioc;
	p->mb_m.m_msg[2] = hiword( iop->io_base);
	p->mb_m.m_msg[3] = loword( iop->io_base);
	hrlink(cp->wtype, p);
	while (not xcheck( self)) {
		cp->sleeping[SLP_WRIT] = cp;
		sleep( cp, CVNOSIG, 0, 0);
		cp->sleeping[SLP_WRIT] = 0;
	}
	cp->flags &= ~WRITING;
	wakeup( &cp->flags);
	if (xstate == XERROR)
		u.u_error = EIO;
	else
		iop->io_ioc = 0;
}



hrioctl( dev, com, args)
dev_t	dev;
int	*args;
{
	register struct mbuf	*p;
	register struct client	*cp;
	register uint		self,
				d;
	uint	*ip;
	ulong	*lp;
	static	 uint	mousepat[MOUSEHI];

	self = minor( dev);

	switch (com) {
	case CIOMLOCK:
		hrlockwait();
		return;
	case CIOMUNLOCK:
		hrlockwake();
		return;
	/* Event-ring doorbell.  The ring index is passed BY VALUE in the argument
	 * word (there is nothing in user memory to copy), so no ucopy here. */
	case CIOEVWAIT:
		hrevwait((int)args);
		return;
	case CIOEVWAKE:
		hrevwake((int)args);
		return;
	case CIOFLUSH:
		hrflush(self);
		return;
	case CIOSENDM:
		xcheck(self);
		d = sphi();
		if (not ismbfree() ) {
			client[self].flags |= BUSY;
			u.u_error = EDBUSY;
			spl(d);
			return;
		}
		p = mbufp;
		mbufp = p->mb_next;
		spl(d);
		ukcopy( args, &p->mb_m, sizeof p->mb_m);
		if (u.u_error)
		{
			hrunalloc(p);
			return;
		}
		d = p->mb_m.m_dst;
		if (d >= DESTMAX)
			baddest( );
		p->mb_m.m_src = self;
		hrlink(d, p);
		return;
	case CIOGETM:
		cp = &client[self];
		if (self==hrsmgr && !mstail && mouse.m_msg[0]==SM_MOUSE) {
			kucopy( &mouse, args, sizeof( struct message));
			mouse.m_msg[0] = not SM_MOUSE;
			return;
		}
		hrlock(self, NEEDMSG);
		while (not cp->head) {
			if (cp->flags&BUSY && ismbfree() ) {
				cp->flags &= ~(BUSY+NEEDMSG);
				u.u_error = EDATTN;
				return;
			}
			hrsleep( &cp->head, self, SLP_MSG);
			if ( SELF->p_ssig && nondsig() ) {
				u.u_error = EINTR;
				cp->flags &= ~(BUSY+NEEDMSG);
				wakeup(&cp->flags);
				return;
			}
			if (self==hrsmgr && !mstail && mouse.m_msg[0]==SM_MOUSE)
			{
				kucopy( &mouse, args, sizeof( struct message));
				mouse.m_msg[0] = not SM_MOUSE;
				cp->flags &= ~NEEDMSG;
				return;
			}
		}
		cp->flags &= ~NEEDMSG;
		kucopy( &cp->head->mb_m, args, sizeof( struct message));
		if (u.u_error)
			return;
#ifdef DEBUG
		if ( self == cp->head->mb_m.m_src )
		{
			struct mbuf *p2;
			printf("\007CIOGETM: msg to self %d\n", self);
			for ( p2=client[SMGR].head; p2 ; p2=p2->mb_next )
				if ( p2 == cp->head )
				{
					printf("\tIN SMGR LIST, TOO\n");
					return;
				}
		}
#endif
		hrfree(self);
		return;
	case CIOGETD:
		while (xstate != XIDLE)
			hrsleep( &x, self, SLP_GETD);
		ukcopy( args, &x, sizeof x);
		if (u.u_error)
			return;
		if (x.x_src >= DESTMAX)
			baddest( );
		cp = &client[x.x_src];
		loop
			switch (xstate) {
			case XIDLE:
				if (not x.x_count) {
					wakeup( &x);
					return;
				}
				xstate = XFILL;
				for (d=0 ; d <= SLP_MAX ; d++)
					if (cp->sleeping[d])
					{
						wakeup( cp->sleeping[d]);
						break;
					}
				break;
			case XFILL:
				sleep( &xstate, CVCLIST, IVCLIST, SVCLIST);
				break;
			case XDRAIN:
				kucopy( xbuf, x.x_dstp, xbufn);
				if (u.u_error) {
					xstate = XIDLE;
					wakeup( &x);
					return;
				}
				x.x_srcp += xbufn;
				x.x_dstp += xbufn;
				x.x_count -= xbufn;
				xstate = XIDLE;
				break;
			case XERROR:
				panic( "CIOGETD");
			}
	case CIOSIG:
		if ( self < WINDOW )
			hrsignal( hiword( args), loword( args));
		return;
	case CIOEVMGR:
		if ( self < WINDOW && self != hrsmgr )
		{
			struct evmgr *ev;

			for ( ev=evmlist ; ev ; ev=ev->enext )
				if ( ev->edev == self )
					return;
			while ( hrislocked )
				sleep(&hrislocked, 0, 0, 0);
			if ( hrsmgr != NOEVMGR )
			{
				ev = evmfree;
				evmfree = evmfree->enext;
				ev->edev = hrsmgr;
				ev->emsen = mousebuf.ms_en;
				ev->enext = evmlist;
				evmlist = ev;
			}
			hrevmgr(self);
		}
		return;
	case CIOUEVMGR:
		if ( self < WINDOW )
			hruevmgr(self);
		return;
	case CIOHOLD:
		if ( self == hrsmgr && self != SMGR && !hrislocked )
		{
			hrislocked++;
			hrsignal(SMGR, SIGALRM);
		}
		return;
	case CIOUHOLD:
		if ( self == hrsmgr )
		{
			hrislocked = 0;
			wakeup(&hrislocked);
		}
		return;
	case CIOWAIT:
		if ( self == SMGR )
			while ( hrislocked )
				sleep( &hrislocked, 0, 0, 0);
		return;
	case CIOMSEON:
		/* Any client may show/hide the cursor, not just the event manager:
		 * the window server AND direct-render clients (which blit their own
		 * content straight to VRAM) must bracket their blits to keep the XOR
		 * cursor from smearing (cooperative single-user model). */
		hrmseon();
		return;
	case CIOMSEOFF:
		hrmseoff();
		return;
	case CIOMOUSE:
		/* Any cursor-control client may set the sprite shape (not just the
		 * event manager): the window server swaps in the move/resize "hand"
		 * during a ghost-drag and restores the arrow after (same cooperative
		 * single-user model as CIOMSEON/OFF above). */
		if ( mousebuf.ms_en )
			hrudraw();
		ukcopy(args, mousepat, sizeof(mousepat));
		if ( !u.u_error )
		{
			mousebuf.ms_bit = 7;
			ip = endof(mousepat) - 1;
			lp = endof(mousebuf.ms_buf) - 1;
			while ( ip >= mousepat )
				*lp-- = ((ulong) *ip--) << 16;
		}
		if ( mousebuf.ms_en )
			hrdraw(mouse.m_msg[2], mouse.m_msg[3]);
		return;
	case CIODATA:
		cp = &client[ hiword( args)];
		cp->datc = loword( args );
		cp->flags &= ~READING;
		wakeup( &cp->datc );
		return;
	case CIOACK:
		cp = &client[ hiword( args)];
		cp->data.i = loword( args );
		cp->flags &= ~NEEDDAT;
		wakeup( &cp->data );
		return;
	case CIOSGTTY:
	case CIOTCHRS:
		ukcopy( args, &tiomsg, sizeof( struct message ));
		if ( u.u_error )
			return;
		cp = &client[tiomsg.m_dst];
		cp->data.tty = * (struct sgttyb *) &tiomsg.m_msg[1];
		cp->flags &= ~NEEDDAT;
		wakeup(&cp->data);
		return;
	case TIOCSETP:
	case TIOCSETC:
	case TIOCSETN:
		if ( self < WINDOW )
			return;
		p = hralloc(self);
		p->mb_m.m_src = self;
		p->mb_m.m_msg[0] = ( com == TIOCSETP ? WM_TIOSETP :
				   ( com == TIOCSETN ? WM_TIOSETN: WM_TIOSETC));
		if ( sizeof(struct sgttyb) >
			sizeof(p->mb_m.m_msg) - sizeof(p->mb_m.m_msg[0]) )
		{
			hrunalloc(p);
			panic("xxx tiocset");
		}
		ukcopy( args, &p->mb_m.m_msg[1], sizeof ( struct sgttyb ) );
		if ( u.u_error )
		{
			hrunalloc(p);
			return;
		}
		hrlink(client[self].wtype, p);
		return;
	case TIOCGETP:
	case TIOCGETC:
		if ( self < WINDOW )
			return;
		cp = &client[self];
		hrlock(self, NEEDDAT);
		p = hralloc(self);
		p->mb_m.m_src = self;
		p->mb_m.m_msg[0] = ( com == TIOCGETP ? WM_TIOGETP : WM_TIOGETC);
		if ( sizeof(struct sgttyb) >
			sizeof(p->mb_m.m_msg) - sizeof(p->mb_m.m_msg[0]) )
		{
			hrunalloc(p);
			panic("xxx tiocget");
		}
		hrlink(cp->wtype, p);
		if ( hrwait(self, SLP_NDAT) == FALSE )
			return;
		kucopy( &cp->data.tty, args, sizeof(struct sgttyb) );
		return;
	case TIOCEXCL:
		client[self].flags |= EXCL;
		return;
	case TIOCNXCL:
		if ( self >= WINDOW )
			client[self].flags &= ~EXCL;
		return;
	case TIOCFLUSH:
		if ( self < WINDOW )
			return;
		p = hralloc(self);
		p->mb_m.m_src = self;
		p->mb_m.m_msg[0] = WM_TIOFLSH;
		hrlink(client[self].wtype, p);
		return;
	}
}


hrsleep( e, self, stype)
char	*e;
uint	self;
uint	stype;
{
	register struct client	*cp;

	if ( stype > SLP_MAX )
		panic("bad sleep type");
	xcheck( self);
	cp = &client[self];
	cp->sleeping[stype] = e;
	sleep( e, CVNOSIG, 0, 0);
	cp->sleeping[stype] = 0;
}


bool
xcheck( self)
uint	self;
{

	if (xstate!=XFILL || x.x_src!=self)
		return (FALSE);
	xbufn = sizeof xbuf;
	if (x.x_count < sizeof xbuf)
		xbufn = x.x_count;
	ukcopy( x.x_srcp, xbuf, xbufn);
	if (u.u_error) {
		xstate = XERROR;
		u.u_error = 0;
		wakeup( &xstate);
		return (TRUE);
	}
	xstate = XDRAIN;
	wakeup( &xstate);
	return (xbufn == x.x_count);
}

#include "hr2.c"

/*
** make this client the event manager.
*/
hrevmgr(self)
register uint self;
{
	register struct mbuf	*p;
	register struct client	*cp;
	int			s;

	s = sphi();
	if ( mstail )
	{
		cp = &client[hrsmgr];
		p = cp->head;
		cp->head = mstail->mb_next;
		cp = &client[self];
		if ( not (mstail->mb_next = cp->head) )
		{
			cp->tail = mstail;
			wakeup(&cp->head);
		}
		cp->head = p;
#ifdef DEBUG
		for ( p=cp->head; p->mb_next ; p=p->mb_next )
			if ( p == mstail )
				printf("HREVMGR: good mstail\n");
		if ( p != cp->tail)
			printf("HREVMGR: bad tail %d\n", self);
#endif
	}
	else if ( mouse.m_msg[0] == SM_MOUSE )
		wakeup(&client[self].head);
	hrsmgr = self;
	spl(s);
}


/*
**  If the client is in the event mgr list, eliminate it.
**  If it is the current event mgr, change to the next one if any.
**  If there is no next mgr, eliminate all mouse msgs and disable.
*/  
hruevmgr(self)
uint	self;
{
	register struct evmgr	*ep,
				*ep2;
	int			s;

	if ( self == hrsmgr )
	{
		if ( not (ep = evmlist) )
		{
			s = sphi();
			mouse.m_msg[0] = not SM_MOUSE;
			hrmsereset(0);		/* no manager: force cursor off, clear hides */
			while ( mstail )
				hrfree(self);
			hrsmgr = NOEVMGR;
			spl(s);
		}
		else
		{
			evmlist = ep->enext;
			ep->enext = evmfree;
			evmfree = ep;
			hrevmgr(ep->edev);
			hrmsereset(ep->emsen);	/* restore new manager's cursor state */
		}
		if ( hrislocked )
		{
			hrislocked = 0;
			wakeup(&hrislocked);
		}
		return;
	}

	ep2 = NULL;
	ep = evmlist;
	while ( ep )
	{
		if ( ep->edev == self )
		{
			if ( ep2 )
				ep2->enext = ep->enext;
			else
				evmlist = ep->enext;
			ep->enext = evmfree;
			evmfree = ep;
			return;
		}
		ep2 = ep;
		ep = ep->enext;
	}
}
