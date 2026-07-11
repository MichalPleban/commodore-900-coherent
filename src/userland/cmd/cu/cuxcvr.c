/*
 * The transceiver code that is used by CU
 * on the remote system to send/receive files.
 */

#include <stdio.h>
#include "cu.h"
#include <sgtty.h>

static	struct sgttyb new, old;

char	*receive();
char	*send();

main(argc, argv)
int argc;
char *argv[];
{
	register int i, estat;
	register char *s;
	register char * (*func)();

	if (argc < 3)
		usage();
	switch (*argv[1]) {
	case 'r':
		func = receive;
		break;

	case 's':
		func = send;
		break;

	default:
		usage();
	}
	init();
	estat = 0;
	for (i = 2; i < argc; i++)
		if ((s = (*func)(argv[i])) != NULL) {
			estat++;
			break;
	}
	endpkt(s==NULL ? "END" : s, 1);
	gexit(estat);
}

/*
 * This code sits on the remote system
 * and acts as a transmitter.  It allows
 * CU to receive a file on its local system.
 */
char *
receive(ifile)
char *ifile;
{
	register int n;
	register unsigned seqno;
	register char *ofile;
	register FILE *fp;

	ofile = basename(ifile);
	if ((n = strlen(ofile)) > NPKT)
		return ("file name too long");
	if ((fp = fopen(ifile, "r")) == NULL)
		return ("cannot open file");
	xpkt.p_type = 'F';
	xpkt.p_len = n + 1 + 3;
	xpkt.p_seq[0] = xpkt.p_seq[1] = 0;
	strncpy(&xpkt.p_data[0], ofile, NPKT);
	sendpkt(&xpkt);

	for (seqno = 0; ; seqno++) {
		xpkt.p_type = 'I';
		xpkt.p_seq[0] = seqno&0xFF;
		xpkt.p_seq[1] = seqno>>8;
		if ((n = fread(xpkt.p_data, 1, NPKT, fp)) <= 0)
			break;
		xpkt.p_len = n + 3;
		sendpkt(&xpkt);
	}
	fclose(fp);
	return (NULL);
}

/*
 * This code sits on the remote system and
 * is the receiver.  It enables CU to send
 * a file to this remote system from its
 * local system.
 */
char *
send(ifile)
char *ifile;
{
	register char *ofile;
	register PKT *rpp;
	register unsigned seqno;
	register int n;
	FILE *fp;

	ofile = basename(ifile);
	if ((n = strlen(ifile)) > NPKT)
		return ("file name too long");
	if ((fp = fopen(ofile, "w")) == NULL)
		return ("cannot create file", 1);
	xpkt.p_type = 'F';
	xpkt.p_len = n + 1 + 3;
	xpkt.p_seq[0] = xpkt.p_seq[1] = 0;
	strncpy(&xpkt.p_data[0], ifile, NPKT);
	putpkt(&xpkt);

	seqno = 0;
	for (;;) {
		rpp = rcvpkt(seqno);
		switch (rpp->p_type) {
		case 'F':
			ackpkt();
			fclose(fp);
			return (NULL);

		case 'I':
			seqno++;
			fwrite(rpp->p_data, 1, rpp->p_len-3, fp);
			break;

		case 'A':
			break;

		default:
			nakpkt();
			continue;
		}
		ackpkt();
	}
	/* NOTREACHED */
}

/*
 * Extract base name from a file.
 */
basename(s)
register char *s;
{
	register char *cp;

	for (cp=s; *cp!='\0'; cp++)
		;
	while (cp>s && *--cp!='/')
		;
	if (*cp == '/')
		cp++;
	return (cp);
}

/*
 * Initialise the serial port (standard output)
 * for sending/receiving of data.
 */
init()
{
	pktinit();
	ioctl(1, TIOCGETP, &old);
	new = old;
	new.sg_flags |= RAW;
	new.sg_flags &= ~(ECHO|CRMOD);
	ioctl(1, TIOCSETN, &new);
}

/*
 * When an end packet is received, come
 * here for common cleanup.
 */
doend(pp)
register PKT *pp;
{
	if (pp != NULL)
		endpkt(pp->p_data, 0);
	gexit(0);
}

/*
 * Exit with status but after
 * resetting the serial port to
 * a known state.
 */
gexit(s)
int s;
{
	ioctl(1, TIOCSETN, &old);
	exit(s);
}

/*
 * Serial put command.
 */
sput(c)
{
	putchar(c);
}

/*
 * Serial get a character.
 */
sget()
{
	return (getchar());
}

usage()
{
	fprintf(stderr, "Usage: /etc/cuxcvr [rs] file ...\n");
	exit(1);
}
