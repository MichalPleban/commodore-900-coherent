/*
 * fifotest.c - reproduce the hrgui event-pipe handshake in isolation.
 *
 * zview answers each client over a NAMED pipe the client makes for itself:
 * the client mknod()s /tmp/hrc<pid>, opens it for READING and blocks; the
 * server then opens it for WRITING, unlink()s it immediately, and writes a
 * 16-byte event.  With two clients, whichever one the server answers SECOND
 * never sees its reply and blocks forever -- so reproduce exactly that shape
 * with no graphics, no server and no window system in the way.
 *
 * Prints one line per child: "child N got <count>".  A healthy run reports 16
 * for every child; a hang reports nothing for the starved one.
 */
#include <ino.h>

#define NKID	4
#define EVSZ	16

char	path[NKID][24];

/* Real GUI clients are 45-60 KB and several run at once, so on a 1 MB
 * machine they swap.  fifotest children were tiny and never did -- give
 * them bulk so the handshake runs under the same memory pressure. */
char	bulk[40000];

static
mkpath(buf, n)
char *buf;
{
	char *p;

	strcpy(buf, "/tmp/ft");
	p = buf;
	while ( *p )
		p++;
	*p++ = '0' + n;
	*p = 0;
}

/* The client half: make the pipe, open it for reading, then block. */
static
child(n)
{
	int fd, got;
	char buf[EVSZ];

	unlink(path[n]);
	if ( mknod(path[n], IFPIPE | 0600, 0) < 0 )
	{
		printf("child %d: mknod failed\n", n);
		_exit(1);
	}
	if ( (fd = open(path[n], 0)) < 0 )
	{
		printf("child %d: open failed\n", n);
		_exit(1);
	}
	printf("child %d ready fd %d\n", n, fd);
	got = read(fd, buf, EVSZ);
	printf("child %d got %d\n", n, got);
	_exit(0);
}

int	cmdr, cmdw;		/* the shared command pipe, as zview has */

/* The client half, in zview's real order: make the pipe and open it for
 * reading, THEN announce yourself on the shared command pipe, THEN block. */
static
child2(n)
{
	int fd, got;
	char buf[EVSZ];
	char rec[EVSZ];

	unlink(path[n]);
	if ( mknod(path[n], IFPIPE | 0600, 0) < 0 || (fd = open(path[n], 0)) < 0 )
	{
		printf("child %d: setup failed\n", n);
		_exit(1);
	}
	for ( got = 0; got < sizeof(bulk); got += 512 )	/* make us swappable */
		bulk[got] = n;
	close(cmdr);
	for ( got = 0; got < EVSZ; got++ )
		rec[got] = n;
	write(cmdw, rec, EVSZ);		/* "connect" */
	got = read(fd, buf, EVSZ);
	printf("child %d got %d\n", n, got);
	_exit(0);
}

main()
{
	int i, fd, nw, pid, n;
	int cp[2];
	char rec[EVSZ];
	char ev[EVSZ];

	for ( i = 0; i < NKID; i++ )
		mkpath(path[i], i);

	if ( pipe(cp) < 0 )
	{
		printf("pipe failed\n");
		exit(1);
	}
	cmdr = cp[0];
	cmdw = cp[1];

	for ( i = 0; i < NKID; i++ )
		if ( (pid = fork()) == 0 )
			child2(i);
	close(cmdw);

	/* The server half: react to each connect as it arrives -- open the
	 * client's pipe for writing, unlink it at once, answer it. */
	for ( i = 0; i < NKID; i++ )
	{
		if ( read(cmdr, rec, EVSZ) != EVSZ )
		{
			printf("server: short connect read\n");
			break;
		}
		n = rec[0];
		if ( n < 0 || n >= NKID )
		{
			printf("server: bad connect %d\n", n);
			continue;
		}
		if ( (fd = open(path[n], 1)) < 0 )
		{
			printf("server: open %s failed\n", path[n]);
			continue;
		}
		unlink(path[n]);
		for ( nw = 0; nw < EVSZ; nw++ )
			ev[nw] = 'a' + n;
		nw = write(fd, ev, EVSZ);
		printf("server: answered %d, wrote %d (fd %d)\n", n, nw, fd);
	}

	sleep(4);
	printf("done\n");
	exit(0);
}
