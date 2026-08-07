/*
 * hrpump.c - tiny I/O pump for the ZView terminal (zterm).
 *
 * zterm multiplexes two blocking input sources -- the pty master's output and
 * its window's event ring -- into one "mux" pipe it owns (GUI.md 4.5),
 * so the main process blocks on a single read().  The V7 way to watch two fds
 * without select() is a dumb copier per source.  zterm used to fork() those
 * copiers, but fork clones its whole data/BSS (the 6 KB character grid + the
 * salvaged engine globals), so each terminal cost THREE ~12 KB processes and a
 * few open terminals exhausted memory (dead 3rd/4th terminal; menu save-buffer
 * malloc failing so the menu never cleared).  Exec'ing this instead makes each
 * pump a ~2 KB libc-only process.
 *
 * Invoked as:  hrpump <role> <masterfd> <muxwfd> [<wid>]
 *   role 'm' (master pump): copy master output -> mux as MX_DATA records; on EOF
 *        emit MX_EOF.
 *   role 'e' (event pump):  drain the window's event ring; a keystroke (E_KEY) is written
 *        STRAIGHT to the master (so ^C is never starved behind a flood of shell
 *        output), everything else is forwarded to the mux as an MX_EVT record.
 * fd numbers are passed by value (no dup2 renumbering); we close everything else
 * so the pump holds no stray references (e.g. the mux read end).
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"

/* Must match zterm.c. */
#define	MX_DATA	0
#define	MX_EVT	1
#define	MX_EOF	2
struct mux {
	char		tag;
	unsigned char	n;
	char		d[16];
};

extern int	atoi();
extern long	hr_sellen();
extern long	hr_cliplen();

/* Ceiling on one paste.  Not a store limit -- the selection may be megabytes in
 * the file store -- but a pty one: ttstash() (drv/tty.c) silently drops anything
 * past NCIB-1 = 255 characters within a single LINE, and a huge paste would also
 * drain the global clist pool that every other tty shares.  So bound it and say
 * so on the console rather than quietly losing the tail. */
#define	PASTEMAX	1024

/* Insert text as if it had been typed.  `clip' picks the store: 0 = the PRIMARY
 * selection (a middle-click), 1 = the clipboard (the window menu's Paste).  Only
 * those two calls differ -- the streaming, the bound and the newline conversion
 * are one mechanism serving both, which is the point of doing it here.
 *
 * Streams -- never holds more than one chunk -- so a file-backed selection from
 * some future editor pastes through the same path as a terminal's own two-line
 * one.
 *
 * '\n' becomes '\r' because that is what a keystroke would have delivered: the
 * line discipline turns CR into NL itself (ISCRMOD), and it is that conversion
 * that makes ttstash push the finished line to the shell. */
static
dopaste(mfd, clip)
{
	char b[64];
	long off, len, want;
	int n, i;

	want = clip ? hr_cliplen() : hr_sellen();
	if ( want <= 0 )
		return;
	len = want > PASTEMAX ? PASTEMAX : want;
	for ( off = 0; off < len; off += n )
	{
		n = clip ? hr_clipread(off, b, sizeof(b))
			 : hr_selread(off, b, sizeof(b));
		if ( n <= 0 )
			break;			/* empty, or replaced under us */
		if ( off + n > len )
			n = (int)(len - off);
		for ( i = 0; i < n; i++ )
			if ( b[i] == '\n' )
				b[i] = '\r';
		if ( write(mfd, b, n) != n )
			break;
	}
	/* Only visible when hrpump is run by hand: under the GUI fd 2 is closed
	 * above, and zview points its own stderr at /dev/null anyway so that
	 * nothing can scribble on the framebuffer it owns. */
	if ( want > len )
		write(2, "hrpump: paste truncated to 1024 bytes\n", 38);
}

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

	/* Drop every inherited fd but ours.  NOTE evfd is NOT an fd any more (it is
	 * the window id for role 'e'), so it must not be spared here. */
	for ( f = 0; f < 20; f++ )
		if ( f != mfd && f != muxw )
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
		/* evfd is no longer a pipe fd: it is our WINDOW ID, and events come
		 * from that window's ring in the shared tail (shmem.h SHM_EVQ).  We
		 * still exist as a separate process for the same reason as before --
		 * zterm must block on ONE thing, so we funnel events into its mux. */
		for (;;)
		{
			hr_evwait(evfd);
			if ( !hr_evget(evfd, (short *)&e) )
				continue;
			if ( e.wm_type == E_KEY )
			{
				if ( e.wm_arg[0] > 0x7f )
					continue;	/* HRK_* function key: no byte
							 * to write to a pty (yet)   */
				ch = e.wm_arg[0] & 0xff;
				write(mfd, &ch, 1);	/* keystroke straight to the shell */
				continue;
			}
			if ( e.wm_type == E_PASTE )
			{
				/* Handled HERE, next to the keystroke path and for the
				 * same reason: this is a separate process, so a paste
				 * that the shell is slow to swallow cannot stall zterm's
				 * rendering loop.  zterm never sees the event.  */
				dopaste(mfd, 0);
				continue;
			}
			if ( e.wm_type == E_MENU )
			{
				/* The window menu's Copy and Paste (wire.h HRM_*, asked
				 * for by zterm's ha_menu).  Both belong here rather than
				 * in zterm: Paste for the reason above, and Copy because
				 * it needs no part of the grid -- the bytes are already
				 * in the PRIMARY store, put there by copysel() when the
				 * drag ended, so a Copy is purely one store to the other.
				 * Neither disturbs the selection: the highlight stays up,
				 * and can be copied again or middle-pasted afterwards. */
				if ( e.wm_arg[0] == HRM_COPY )
					hr_clipfromsel();
				else if ( e.wm_arg[0] == HRM_PASTE )
					dopaste(mfd, 1);
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
