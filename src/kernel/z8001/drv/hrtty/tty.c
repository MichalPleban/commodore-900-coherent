/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*	     
 * This is the common part of
 * typewriter service. It handles all
 * device independent aspects of
 * a typewriter, including tandem flow
 * control, erase and kill, stop and
 * start and common ioctl functions.
 */
#include <coherent.h>
#include <clist.h>
#include <proc.h>
#include <uproc.h>
#include <errno.h>
#include <sched.h>
#include <io.h>
#include <tty.h>
#include <deftty.h>
#include <stat.h>
#include <drvcon.h>

/*
 * Tty open.
 * Called by driver on first open.
 * Set up defaults.
 */
ttopen(tp)
register TTY *tp;
{
	tp->t_escape = 0;
#ifdef	OLD	/* How old? */
	tp->t_sgttyb.sg_ispeed = tp->t_dispeed;
	tp->t_sgttyb.sg_ospeed = tp->t_dospeed;
#else
	tp->t_sgttyb.sg_ispeed = DEF_SG_ISPEED;	 /* default speed settings */
	tp->t_sgttyb.sg_ospeed = DEF_SG_OSPEED;
#endif
	tp->t_sgttyb.sg_erase  = DEF_SG_ERASE;
	tp->t_sgttyb.sg_kill   = DEF_SG_KILL;
	tp->t_sgttyb.sg_flags  = DEF_SG_FLAGS;
	tp->t_tchars.t_intrc   = DEF_T_INTRC;
	tp->t_tchars.t_quitc   = DEF_T_QUITC;
	tp->t_tchars.t_startc  = DEF_T_STARTC;
	tp->t_tchars.t_stopc   = DEF_T_STOPC;
	tp->t_tchars.t_eofc    = DEF_T_EOFC;
	tp->t_tchars.t_brkc    = DEF_T_BRKC;
	if (tp->t_param != NULL)
		(*tp->t_param)(tp);
}

/*
 * ttsetgrp - set process group when process
 * does not have one.
 * Also set up process's controlling terminal.
 */
ttsetgrp(tp, ctdev)
register TTY *tp;
dev_t ctdev;
{
	register PROC *pp;

	pp = SELF;
	if (pp->p_group == 0) {
		if (tp->t_group == 0)
			tp->t_group = pp->p_pid;
		pp->p_group = tp->t_group;
	}
	if (pp->p_ttdev == NODEV)
		pp->p_ttdev = ctdev;
}

/*
 * Tty close.
 * Called by driver on the last
 * close. Wait for all pending output
 * to go out. Kill input.
 */
ttclose(tp)
register TTY *tp;
{
	register int s;

	while (tp->t_oq.cq_cc != 0) {
		s = sphi();
		if (tp->t_oq.cq_cc != 0) {
			tp->t_flags |= T_DRAIN;
			sleep((char *)&tp->t_oq, CVTTOUT, IVTTOUT, SVTTOUT);
		}
		spl(s);
		if (SELF->p_ssig && nondsig())
		   break;	/* give up the drain, don't spin on the signal */
	}
	ttflush(tp);
	tp->t_flags = tp->t_group = 0;
}

/*
 * Read routine.
 * In cooked mode, copy up to the first newline or
 * break character, or until the count runs out.
 * In CBREAK or RAW modes, return when count runs out
 * or when input clist is empty and we're returning
 * at least one byte.
 */
ttread(tp, iop, s)
register TTY *tp;
register IO *iop;
{
	register c;
	int o;
	int sioc = iop->io_ioc;  /* number of bytes to read */

	while (iop->io_ioc) {
		o = spl(s);
		while ((c = getq(&tp->t_iq)) < 0) {

			/* If we're in CBREAK or RAW mode, and we don't */
			/* have the special "blocking read" bit set for */
			/* these modes, and we read at least one byte   */
		        /* of input, return immediately, since we have  */
			/* run out of characters from the clist.	*/

			if (ISBBYB && ((tp->t_flags & T_BRD) == 0)
			   && iop->io_ioc < sioc)
			{  spl(o);
			   return;
			}

			tp->t_flags |= T_INPUT;  /* wait for more data */
			sleep((char *)&tp->t_iq, CVTTIN, IVTTIN, SVTTIN);
		}
		if (tp->t_iq.cq_cc <= ILOLIM) {
			if ((tp->t_flags&T_ISTOP) != 0)
				tp->t_flags &= ~T_ISTOP;
			if ((tp->t_flags&T_TSTOP) != 0) {
				tp->t_flags &= ~T_TSTOP;
				while (putq(&tp->t_oq, startc) < 0) {
					ttstart(tp);
					waitq();
				}
				ttstart(tp);
			}
		}
		spl(o);
		if (!ISBBYB && ISEOF)
			return;
		if (ioputc(c, iop) < 0)
			return;
		if (!ISBBYB && (c=='\n' || ISBRK))
			return;
	}
}

/*
 * Write routine.
 * Transfer stuff to the character
 * list.
 */
ttwrite(tp, iop, s)
register TTY *tp;
register IO *iop;
{
	register c;
	int o;

	while ((c = iogetc(iop)) >= 0) {
		o = spl(s);
		while (tp->t_oq.cq_cc >= OHILIM) {
			ttstart(tp);
			if (tp->t_oq.cq_cc < OHILIM)
				break;
			tp->t_flags |= T_HILIM;
			sleep((char *)&tp->t_oq, CVTTOUT, IVTTOUT, SVTTOUT);
			if (SELF->p_ssig && nondsig()) {
				u.u_error = EINTR;
				spl(o);
				return;
			}
		}
		while (putq(&tp->t_oq, c) < 0) {
			ttstart(tp);
			waitq();
		}
		spl(o);
	}
	o = spl(s);
	ttstart(tp);
	spl(o);
}

/*
 * This routine handles common
 * typewriter ioctl functions. Note that
 * flushing the stream now means drain the
 * output and clear the input.
 */
ttioctl(tp, com, vec)
register TTY *tp;
register struct sgttyb *vec;
{
	register int	flush = 0;
	register int	drain = 0;
		 int    rload = 0;
		 int	s;

	switch (com) {
	case TIOCQUERY:
		kucopy(&tp->t_iq.cq_cc, vec, sizeof(int));
		break;
	case TIOCGETP:
		kucopy(&tp->t_sgttyb, vec, sizeof (struct sgttyb));
		break;
	case TIOCSETP:
	        ++flush;          /* flush input */
		++drain;	  /* delay for output */
	case TIOCSETN:
		++rload;
		ukcopy(vec, &tp->t_sgttyb, sizeof (struct sgttyb));
		break;
	case TIOCGETC:
		kucopy(&tp->t_tchars, vec, sizeof (struct tchars));
		break;
	case TIOCSETC:
		++rload;
		++drain;
		ukcopy(vec, &tp->t_tchars, sizeof (struct tchars));
		break;
	case TIOCEXCL:
		tp->t_flags |= T_EXCL;
		break;
	case TIOCNXCL:
		tp->t_flags &= ~T_EXCL;
		break;
	case TIOCHPCL:		/* set hangup on last close */
		tp->t_flags |= T_HPCL;
		break;	
	case TIOCCHPCL:		/* don't hangup on last close */
		if (!super())   /* only superuser may do this */
		   u.u_error = EPERM;        /* not su */ 
		else
 	   	   tp->t_flags &= ~T_HPCL;   /* turn off hangup bit */
		break;
	case TIOCGETTF:		/* get tty flag word */
		kucopy(&tp->t_flags, (unsigned *) vec, sizeof(unsigned));
		break;	
	case TIOCFLUSH:
		++flush;        /* flush both input and output */
		break;		/* discarding output: nothing to drain for */
	case TIOCBREAD:		/* blocking read for CBREAK/RAW mode */
		tp->t_flags |= T_BRD;		
		break;
	case TIOCCBREAD:	/* turn off CBREAK/RAW blocking read mode */
		tp->t_flags &= ~T_BRD;		
		break;
	default:
		u.u_error = EINVAL;
	}
	if (drain != 0) {
		/*
		 * Make sure output can actually move before we wait for it.
		 * Entering RAWIN with output suspended by an X-OFF would
		 * otherwise wedge here forever.
		 */
		if (ISRIN && (tp->t_flags&T_STOP) != 0) {
			s = sphi();
			tp->t_flags &= ~T_STOP;
			ttstart(tp);
			spl(s);
		}
		while (tp->t_oq.cq_cc != 0) {
			tp->t_flags |= T_DRAIN;
			sleep((char *)&tp->t_oq, CVTTOUT, IVTTOUT, SVTTOUT);
			if (SELF->p_ssig && nondsig()) {
				u.u_error = EINTR;
				break;	/* don't spin on the pending signal */
			}
		}
	}
	if (flush != 0)
		ttflush(tp);
	if (rload != 0 && tp->t_param != NULL)
		(*tp->t_param)(tp);
}

/*
 * Pull a character from the
 * output queues of the typewriter.
 * Doing fills, newline insert,
 * tab expansion and all other good
 * things. If the stream is empty
 * return a -1.
 * Called at high priority.
 */
ttout(tp)
register TTY *tp;
{
	register c;

	if (tp->t_nfill) {
		--tp->t_nfill;
		c = tp->t_fillb;
	} else if ((tp->t_flags&T_INL) != 0) {
		tp->t_flags &= ~T_INL;
		c = '\n';
	} else {
		if ((c=getq(&tp->t_oq)) < 0)
			return (-1);
		if (!ISROUT) {
			if (c=='\n' && ISCRMOD) {
				tp->t_flags |= T_INL;
				c = '\r';
			} else if (c=='\t' && ISXTABS) {
				tp->t_nfill = ~(tp->t_hpos|~07);
				tp->t_fillb = ' ';
				c = ' ';
			}
		}
	}
	if (!ISROUT) {
		if (c == '\b') {
			if (tp->t_hpos)
				--tp->t_hpos;
		} else if (c == '\r')
			tp->t_hpos = 0;
		else if (c == '\t')
			tp->t_hpos = (tp->t_hpos|07) + 1;
		else if (c>=' ' && c<='~')
			++tp->t_hpos;
	}
	return (c);
}

/*
 * Pass a character to the
 * device independent typewriter
 * routines. Handle erase and
 * kill, tandem flow control things
 * and other magic.
 * Called at high priority from 
 * the driver's interrupt processor.
 */
ttin(tp, c)
register TTY *tp;
register c;
{
	int dc, i, n;

	if (!ISRIN) {
		c &= 0177;
		if (ISINTR) {
			ttsignal(tp, SIGINT);
			return;
		}
		if (ISQUIT) {
			ttsignal(tp, SIGQUIT);
			return;
		}
		if (ISSTOP) {
			if ((tp->t_flags&T_STOP) == 0)
				tp->t_flags |= T_STOP;
			return;
		} 
		if (ISSTART) {
			tp->t_flags &= ~T_STOP;
			ttstart(tp);
			return;
		}
	}
	if ((tp->t_flags&T_ISTOP) != 0)
		return;
	if (!ISRIN) {
		if (c=='\r' && ISCRMOD)
			c = '\n';
		if (tp->t_escape != 0) {
			if (c == ESC)
				++tp->t_escape;
			else {
				if (ISERASE || ISKILL) {
					c |= 0200;
					--tp->t_escape;
				}
				while (tp->t_escape!=0 && tp->t_ibx<NCIB-1) {
					tp->t_ib[tp->t_ibx++] = ESC;
					--tp->t_escape;
				}
				ttstash(tp, c);
			}
			if (ISECHO) {
				putq(&tp->t_oq, c&0177);
				ttstart(tp);
			}
			return;
		}
		if (ISERASE && !ISCBRK) {
			while (tp->t_escape!=0 && tp->t_ibx<NCIB-1) {
				tp->t_ib[tp->t_ibx++] = ESC;
				--tp->t_escape;
			}
			if (tp->t_ibx == 0)
				return;
			dc = tp->t_ib[--tp->t_ibx];
			if (ISECHO) {
				if (!ISCRT)
					putq(&tp->t_oq, c);
				/* don't erase for bell, null, or rubout */
				else if (((c = dc&0177) == '\007')  
					|| c == 0 || c == 0177)		
				        return;
				else if (c != '\b' && c != '\t') {
					putq(&tp->t_oq, '\b');
					putq(&tp->t_oq,  ' ');
					putq(&tp->t_oq, '\b');
				} else if (c == '\t') {
					n = tp->t_opos + tp->t_escape;
					for (i=0; i<tp->t_ibx; ++i) {
						c = tp->t_ib[i];
						if ((c&0200) != 0) {
							++n;
							c &= 0177;
						}
						if (c == '\b')
							--n;
						else {
							if (c == '\t')
								n |= 07;
							++n;
						}
					}
					while (n++ < tp->t_hpos)
						putq(&tp->t_oq, '\b');
				}
				if ((dc&0200) != 0) {
					if ((dc&0177) != '\b')
						putq(&tp->t_oq, '\b');
					putq(&tp->t_oq,  ' ');
					putq(&tp->t_oq, '\b');
				}
				ttstart(tp);
			}
			return;
		}
		if (ISKILL && !ISCBRK) {
			tp->t_ibx = 0;
			tp->t_escape = 0;
			if (ISECHO) {
				if (c < 0x20) {
					putq(&tp->t_oq, '^');
					c += 0x40;
				}
				putq(&tp->t_oq, c);
				putq(&tp->t_oq, '\n');
				ttstart(tp);
			}
			return;
		}
	}
	if (ISBBYB) {
		putq(&tp->t_iq, c);
		if ((tp->t_flags&T_INPUT) != 0) {
			tp->t_flags &= ~T_INPUT;
			wakeup((char *)&tp->t_iq);
		}
	} else {
		if (tp->t_ibx == 0)
			tp->t_opos = tp->t_hpos;
		if (c == ESC)
			++tp->t_escape;
		else
			ttstash(tp, c);
	}
	if (ISECHO) {
		if (ISRIN || !ISEOF) {
			putq(&tp->t_oq, c);
			ttstart(tp);
		}
	}
	if ((n=tp->t_iq.cq_cc)>=IHILIM)
		tp->t_flags |= T_ISTOP;
	else if (ISTAND && (tp->t_flags&T_TSTOP)==0 && n>=ITSLIM) {
		tp->t_flags |= T_TSTOP;
		putq(&tp->t_oq, stopc);
		ttstart(tp);
	}
}

/*
 * Cooked mode.
 * Put character in the buffer and
 * check for end of line. Only a legal
 * end of line can take the last character
 * position.
 */
ttstash(tp, c)
register TTY *tp;
{
	register char *p1, *p2;

	if (c=='\n' || ISEOF || ISBRK) {
		p1 = &tp->t_ib[0];
		p2 = &tp->t_ib[tp->t_ibx];
		*p2++ = c;			/* Always room */
		while (p1 < p2)
			putq(&tp->t_iq, (*p1++)&0177);
		tp->t_ibx = 0;
		tp->t_escape = 0;
		if ((tp->t_flags&T_INPUT) != 0) {
			tp->t_flags &= ~T_INPUT;
			wakeup((char *)&tp->t_iq);
		}
	} else if (tp->t_ibx < NCIB-1)
		tp->t_ib[tp->t_ibx++] = c;
}

/*
 * Start output on a tty.
 * Duck out if stopped.
 * Do wakeups.
 */
ttstart(tp)
register TTY *tp;
{
	register int n;

	n = tp->t_flags;
	if ((n&T_STOP) == 0) {
		if ((n&T_DRAIN)!=0 && tp->t_oq.cq_cc==0
		   && (n&T_INL)==0 && tp->t_nfill==0)
		{	tp->t_flags &= ~T_DRAIN;
			wakeup((char *) &tp->t_oq);
			return;
		}
		(*tp->t_start)(tp);
		if ((n&T_HILIM)!=0 && tp->t_oq.cq_cc<=OLOLIM) {
		   	tp->t_flags &= ~T_HILIM;
			wakeup((char *) &tp->t_oq);
		}
	}
}

/*
 * Flush a tty.
 * Called to clear out queues.
 */
ttflush(tp)
register TTY *tp;
{
    clrq(&tp->t_iq);    
    clrq(&tp->t_oq);      

    tp->t_ibx = 0;	
    tp->t_escape = 0;
    tp->t_flags &= T_SAVE;  /* reset most flag bits */

    wakeup((char *) &tp->t_iq);
    wakeup((char *) &tp->t_oq);
}

/*
 * Send a signal to every process in the given process group.
 */
ttsignal(tp, sig)
TTY *tp;
int sig;
{
	register int g;
	register PROC *pp;

	g = tp->t_group;
	if (g == 0)
		return;
	ttflush(tp);
	pp = &procq;
	while ((pp=pp->p_nforw) != &procq)
		if (pp->p_group == g)
			sendsig(sig, pp);
}

/*
 * Flag hangup internally to force errors
 * on tty read/write, flush tty, then send hangup signal.
 */
tthup(tp)
register TTY *tp;
{
	tp->t_flags &= ~T_CARR;  /* indicate no carrier */
	ttflush(tp);
	ttsignal(tp, SIGHUP);
}

