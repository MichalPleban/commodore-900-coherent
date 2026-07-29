/*
 * hrpump.c - tiny I/O pump for the hrgui terminal (wterm).
 *
 * wterm multiplexes two blocking input sources -- the pty master's output and
 * the window server's event pipe -- into one "mux" pipe it owns (GUI.md 4.5),
 * so the main process blocks on a single read().  The V7 way to watch two fds
 * without select() is a dumb copier per source.  wterm used to fork() those
 * copiers, but fork clones its whole data/BSS (the 6 KB character grid + the
 * salvaged engine globals), so each terminal cost THREE ~12 KB processes and a
 * few open terminals exhausted memory (dead 3rd/4th terminal; menu save-buffer
 * malloc failing so the menu never cleared).  Exec'ing this instead makes each
 * pump a ~2 KB libc-only process.
 *
 * Invoked as:  hrpump <role> <masterfd> <muxwfd> [<evfd>]
 *   role 'm' (master pump): copy master output -> mux as MX_DATA records; on EOF
 *        emit MX_EOF.
 *   role 'e' (event pump):  read server events; a keystroke (E_KEY) is written
 *        STRAIGHT to the master (so ^C is never starved behind a flood of shell
 *        output), everything else is forwarded to the mux as an MX_EVT record.
 * fd numbers are passed by value (no dup2 renumbering); we close everything else
 * so the pump holds no stray references (e.g. the mux read end).
 */
#include <stdio.h>
#include "wire.h"

/* Must match wterm.c. */
#define	MX_DATA	0
#define	MX_EVT	1
#define	MX_EOF	2
struct mux {
	char		tag;
	unsigned char	n;
	char		d[16];
};

extern int	atoi();

main(argc, argv)
char **argv;
{
	struct mux	mx;
	WMSG		e;
	int		role, mfd, muxw, evfd, f, n, i;
	char		ch;

	if ( argc < 4 )
		_exit(1);
	role = argv[1][0];
	mfd  = atoi(argv[2]);
	muxw = atoi(argv[3]);
	evfd = (argc > 4) ? atoi(argv[4]) : -1;

	for ( f = 0; f < 20; f++ )		/* drop every inherited fd but ours */
		if ( f != mfd && f != muxw && f != evfd )
			close(f);

	if ( role == 'm' )
	{
		for (;;)
		{
			n = read(mfd, mx.d, sizeof(mx.d));
			if ( n <= 0 )
				break;
			mx.tag = MX_DATA;
			mx.n = n;
			write(muxw, &mx, sizeof(mx));
		}
		mx.tag = MX_EOF;
		mx.n = 0;
		write(muxw, &mx, sizeof(mx));
	}
	else					/* 'e' */
	{
		for (;;)
		{
			n = read(evfd, &e, sizeof(e));
			if ( n <= 0 )
				break;
			if ( n != sizeof(e) )
				continue;
			if ( e.wm_type == E_KEY )
			{
				ch = e.wm_arg[0] & 0xff;
				write(mfd, &ch, 1);	/* keystroke straight to the shell */
				continue;
			}
			mx.tag = MX_EVT;
			mx.n = sizeof(e);
			for ( i = 0; i < sizeof(e); i++ )
				mx.d[i] = ((char *)&e)[i];
			write(muxw, &mx, sizeof(mx));
		}
	}
	_exit(0);
}
