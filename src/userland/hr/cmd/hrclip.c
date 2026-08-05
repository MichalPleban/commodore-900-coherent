/*
 * hrclip.c - read or set the hrgui text stores from a shell.
 *
 *	hrclip -o		write the current SELECTION to stdout
 *	hrclip -i		make stdin the selection
 *	hrclip -O		write the CLIPBOARD to stdout
 *	hrclip -I		make stdin the clipboard
 *	hrclip -c		copy the selection to the clipboard (menu "Copy")
 *
 * Lower case is the PRIMARY selection (mouse-select, middle-click paste), upper
 * case the clipboard the window menu's Copy and Paste use (shmem.h HRCLIP_PATH).
 * They are separate stores on purpose, so `hrclip -O' is unaffected by whatever
 * the mouse has selected since, and -i does not disturb a clipboard.
 *
 * Two reasons this exists.  It makes both stores scriptable from inside a
 * terminal window (`hrclip -o | ...', `... | hrclip -I'), which is a real
 * facility on a machine whose GUI is mostly shells.  And it makes them testable
 * on the plain serial emulator with --input, before any of the mouse plumbing
 * exists -- no window server, no mouse choreography.
 *
 * On the way IN hrclip is the selection's owner, so per inc/shmem.h it is hrclip
 * that picks the store: it reads one byte more than the tail store holds and
 * chooses from whether that fit.  The choice is still made before hr_selopen, so
 * nothing migrates between stores mid-stream.  On the way OUT there is nothing to
 * pick -- hr_selread resolves the store itself.
 *
 * No stdio: read/write are all this needs, and it keeps the binary small.
 */
#include "shmem.h"
#include "wire.h"

#define CHUNK	512

/* Tell the server the selection changed hands, so whichever window is still
 * showing a highlight for the OLD one drops it (wire.h C_SELOWN/E_SELCLEAR).
 * hrclip is not a GUI client and has no window, so it claims ownership as
 * wid -1: the server clears the previous owner and records that no window holds
 * the selection now.  The shared command pipe is inherited down the process
 * tree, so this works from a shell inside a terminal window; run outside the
 * GUI the fd is not a command pipe and the write simply fails, which is fine. */
static
claimed()
{
	WMSG c;
	int i;

	c.wm_type = C_SELOWN;
	c.wm_wid = -1;
	for ( i = 0; i < WM_NARG; i++ )
		c.wm_arg[i] = 0;
	write(HR_CMDFD, &c, sizeof(c));
	return 0;
}

static char	buf[HRSEL_INL + 1];	/* one more than the tail store holds */
static char	obuf[CHUNK];

static
usage()
{
	write(2, "usage: hrclip -i | -o | -I | -O | -c\n", 36);
	exit(1);
}

/* Copy the selection to stdout, whichever store holds it. */
static
dumpsel()
{
	long off, len;
	int n;

	len = hr_sellen();
	for ( off = 0; off < len; off += n )
	{
		n = hr_selread(off, obuf, sizeof(obuf));
		if ( n < 0 )
		{
			write(2, "hrclip: selection changed while reading\n", 40);
			return 1;
		}
		if ( n == 0 )
			break;
		if ( write(1, obuf, n) != n )
			return 1;
	}
	return 0;
}

/* Make stdin the selection.  The first read decides the store. */
static
setsel(wid)
{
	int n, k;

	n = 0;
	while ( n < sizeof(buf) && (k = read(0, buf + n, sizeof(buf) - n)) > 0 )
		n += k;
	if ( k < 0 )
	{
		write(2, "hrclip: read failed\n", 20);
		return 1;
	}

	if ( n <= HRSEL_INL )			/* it fits: the tail store */
	{
		if ( hr_selopen(wid, HRSEL_MEM) < 0 )
			goto busy;
		hr_selwrite(buf, n);
		if ( hr_selclose() < 0 )
			goto failed;
		claimed();
		return 0;
	}

	/* Bigger than the tail store, so the file store -- declared up front, with
	 * the bytes we have already drained written first and the rest streamed. */
	if ( hr_selopen(wid, HRSEL_FILE) < 0 )
		goto busy;
	if ( hr_selwrite(buf, n) < 0 )
		goto failed;
	while ( (k = read(0, buf, sizeof(buf))) > 0 )
		if ( hr_selwrite(buf, k) < 0 )
			goto failed;
	if ( hr_selclose() < 0 )
		goto failed;
	claimed();
	return 0;

busy:
	write(2, "hrclip: selection busy\n", 23);
	return 1;
failed:
	hr_selclose();
	write(2, "hrclip: write failed\n", 21);
	return 1;
}

/* Copy the clipboard to stdout.  No store to resolve and no generation to lose a
 * race with: hr_cliplen pins the snapshot and the reads stream it. */
static
dumpclip()
{
	long off, len;
	int n;

	len = hr_cliplen();
	for ( off = 0; off < len; off += n )
	{
		n = hr_clipread(off, obuf, sizeof(obuf));
		if ( n < 0 )
		{
			write(2, "hrclip: clipboard read failed\n", 30);
			return 1;
		}
		if ( n == 0 )
			break;
		if ( write(1, obuf, n) != n )
			return 1;
	}
	return 0;
}

/* Make stdin the clipboard.  One store, so nothing to choose: it is streamed
 * straight through, and the previous clipboard stands until the last byte is
 * safely written (hr_clipclose swaps it in). */
static
setclip()
{
	int k, bad;

	if ( hr_clipopen() < 0 )
	{
		write(2, "hrclip: cannot write clipboard\n", 31);
		return 1;
	}
	bad = 0;
	while ( (k = read(0, buf, sizeof(buf))) > 0 )
		if ( hr_clipwrite(buf, k) < 0 )
		{
			bad = 1;		/* hr_clipclose will discard it all */
			break;
		}
	if ( k < 0 )
		bad = 1;		/* stdin died: what we did read is published */
	if ( hr_clipclose() < 0 )
		bad = 1;
	if ( bad )
	{
		write(2, "hrclip: clipboard write failed\n", 31);
		return 1;
	}
	return 0;
}

main(argc, argv)
char **argv;
{
	if ( argc != 2 || argv[1][0] != '-' || argv[1][2] != '\0' )
		usage();
	/* The SELECTION's header lives in the tail of the hi-res card's VRAM, so on a
	 * machine with no such card it is open bus: every store would vanish and every
	 * load float to 0xFF, and hrclip would appear to work while doing nothing.  Say
	 * so instead.  The clipboard is only a file, so -I and -O are exempt: they work
	 * on any machine, which also makes the clipboard testable on the plain serial
	 * emulator.  -c touches both and so needs the card. */
	switch ( argv[1][1] )
	{
	case 'O':
		exit(dumpclip());
	case 'I':
		exit(setclip());
	}
	if ( !hr_selok() )
	{
		write(2, "hrclip: no shared memory (no hi-res graphics card)\n", 51);
		exit(2);
	}
	switch ( argv[1][1] )
	{
	case 'o':
		exit(dumpsel());
	case 'i':
		exit(setsel(-1));	/* no window: hrclip is not a GUI client */
	case 'c':			/* what the menu's Copy does */
		exit(hr_clipfromsel() < 0 ? 1 : 0);
	}
	usage();
}
