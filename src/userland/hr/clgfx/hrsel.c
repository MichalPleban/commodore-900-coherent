/*
 * hrsel.c - the hrgui text stores: the PRIMARY selection (select-to-copy /
 * middle-paste) and the CLIPBOARD (window-menu Copy / Paste).
 *
 * Most of this file is the first; the clipboard is a short section at the end,
 * built out of the same file plumbing but sharing no state with it (shmem.h
 * HRCLIP_PATH says why the two are deliberately unalike).  Everything below
 * until that section is about the selection.
 *
 * The layout and the reasoning behind the two stores are in inc/shmem.h; this is
 * the implementation.  The two invariants worth restating here, because every
 * function below exists to keep one of them:
 *
 *  1. A selection lives WHOLLY in the tail or WHOLLY in a file.  The owner picks
 *     at hr_selopen() and the library never migrates one to the other.
 *  2. NO file I/O ever happens under the selection lock, and the selection lock
 *     is never the drawing lock -- a copy must not stall the whole UI.  This is
 *     what the mode argument buys: because the store is known before the first
 *     byte, the FILE path can write and close unlocked and take the lock only to
 *     publish the header.
 *
 * Readers take no lock at all (seqlock retry), so a slow paste never blocks a
 * copy either -- and they resolve sq_file themselves, so a caller pastes from
 * either store with one code path.  That is what lets zterm, which only ever
 * writes HRSEL_MEM, paste a large file-backed selection from some future editor.
 *
 * No stdio: this links into hrpump, a deliberately tiny libc-only helper (see the
 * comment at the head of zterm/hrpump.c), so the path is built by hand exactly as
 * hrapp.c's setevpath does rather than dragging in sprintf.
 */
#include "shmem.h"

#define SEL		hr_prim()
#define SELLOCK		((short *)(HRTAIL + SHM_SELLOCK))
#define SELPID		(*(short *)(HRTAIL + SHM_SELPID))
#define SELTIME		(*(long *)(HRTAIL + SHM_SELTIME))

/* A holder never keeps the lock for even one whole second (the guarded sections
 * are a bounded memcpy loop and a handful of word stores), so a word still held
 * after this many seconds belongs to a process that died holding it. */
#define STALESEC	3

/* Bounded spin before we consider stealing.  Contention is a human-rate event on
 * a single-user machine, so a real one resolves within a few iterations; this cap
 * only bounds the pathological case. */
#define SPINMAX		20000

/* Reader retry cap.  A MEM writer holds the lock across its whole streaming
 * loop, so a reader can legitimately see an odd sq_seq for a moment; past this
 * we give up and report an empty selection rather than spin forever. */
#define READTRY		20000

extern int	hr_tas();
extern long	time();
extern long	lseek();

static int	mypid;

/* writer state, valid between hr_selopen and hr_selclose */
static int	wopen;			/* 1 = a write is in progress          */
static int	wmode;			/* HRSEL_MEM / HRSEL_FILE              */
static int	werr;			/* a write failed: publish nothing     */
static long	wlen;			/* bytes accumulated so far            */
static int	wfd = -1;		/* file store: the fd being written    */
static int	wowner;			/* wid to stamp into sq_owner          */
static char	wpath[24];

/* reader state: one cached fd for the file store, so a paste streaming the body
 * in small chunks does not re-open per chunk.  Keyed on the writer pid AND the
 * generation, so it is dropped the moment the selection changes underneath us. */
static int	rfd = -1;
static int	rpid;
static int	rgen;

/* <pfx><pid> without stdio (hrapp.c setevpath does the same).  Used for both
 * stores: HRSEL_PFX<pid> is a selection body, HRCLIP_NEW<pid> a Copy that has
 * not been swapped into place yet. */
static
pidpath(buf, pfx, pid)
register char *buf;
register char *pfx;
register int pid;
{
	register char *p;
	char digits[8];
	register int n;

	for ( p = pfx; *p; p++ )
		*buf++ = *p;
	n = 0;
	if ( pid <= 0 )
		digits[n++] = '0';
	while ( pid > 0 )
	{
		digits[n++] = '0' + (pid % 10);
		pid /= 10;
	}
	while ( n > 0 )
		*buf++ = digits[--n];
	*buf = '\0';
}

static
unlinkpid(pid)
{
	char path[24];

	if ( pid > 0 )
	{
		pidpath(path, HRSEL_PFX, pid);
		unlink(path);
	}
	return 0;
}

/* Acquire the selection lock.  Spins (bounded), then steals a word whose holder
 * has plainly died with it -- see the SHM_SELTIME note in shmem.h.  Returns 0 on
 * success, -1 if the lock could not be had, in which case the caller abandons
 * the copy: losing a selection is harmless, wedging on one is not. */
static
sellock()
{
	register long n;
	long now;

	if ( mypid == 0 )
		mypid = getpid();
	for ( n = 0; n < SPINMAX; n++ )
	{
		if ( hr_tas(SELLOCK) == 0 )	/* was free -> now ours, no syscall */
		{
			SELPID = mypid;
			SELTIME = time((long *)0);
			return 0;
		}
	}
	/* Still held after the spin.  TSET has left the word held either way, so if
	 * the stamp says the holder has had it for seconds, simply adopt it. */
	now = time((long *)0);
	if ( now - SELTIME >= STALESEC )
	{
		SELPID = mypid;
		SELTIME = now;
		return 0;
	}
	return -1;
}

static
selunlock()
{
	SELPID = 0;
	SELTIME = 0;
	*SELLOCK = 0;
}

/* ------------------------------------------------------------------ */
/* writer                                                             */
/* ------------------------------------------------------------------ */

/* Declare a new selection and which store holds it.  HRSEL_MEM takes the lock
 * here and holds it across the caller's streaming loop -- that loop is pure
 * memory, no system call.  HRSEL_FILE takes NOTHING: it writes its file first and
 * locks only in hr_selclose, once the I/O is already done. */
hr_selopen(wid, mode)
{
	if ( wopen )
		return -1;
	if ( mypid == 0 )
		mypid = getpid();
	if ( SEL->sq_magic != HRSEL_MAGIC )	/* never used since power-on */
		hr_selinit();
	wmode = mode;
	wowner = wid;
	wlen = 0;
	werr = 0;
	if ( mode == HRSEL_FILE )
	{
		pidpath(wpath, HRSEL_PFX, mypid);
		if ( (wfd = creat(wpath, 0600)) < 0 )
			return -1;
		wopen = 1;
		return 0;
	}
	if ( sellock() < 0 )
		return -1;
	SEL->sq_seq++;			/* -> odd: a writer is publishing */
	wopen = 1;
	return 0;
}

/* Append to whichever store was declared.  Overflowing the tail store is a
 * CALLER error, not a cue to spill: an owner that cannot bound its content must
 * declare HRSEL_FILE.  The error is latched so hr_selclose publishes nothing. */
hr_selwrite(buf, len)
char *buf;
{
	register char *d;
	register char *s;
	register int i;

	if ( !wopen || len < 0 )
		return -1;
	if ( werr )
		return -1;
	if ( wmode == HRSEL_FILE )
	{
		if ( len && write(wfd, buf, len) != len )
		{
			werr = 1;
			return -1;
		}
		wlen += len;
		return 0;
	}
	if ( wlen + len > HRSEL_INL )
	{
		werr = 1;
		return -1;
	}
	d = SEL->sq_data + (int)wlen;
	s = buf;
	for ( i = 0; i < len; i++ )
		*d++ = *s++;
	wlen += len;
	return 0;
}

/* Publish, and drop the file the previous owner left behind.  On a latched error
 * nothing is published -- except that a failed MEM write has already overwritten
 * the head of sq_data, so that case publishes an EMPTY selection rather than
 * leaving readers pointed at a corrupted mixture of old and new bytes. */
hr_selclose()
{
	register HRSEL *s;
	int oldfile, bad;

	if ( !wopen )
		return -1;
	s = SEL;
	bad = werr;
	wopen = 0;
	werr = 0;

	if ( wmode == HRSEL_FILE )
	{
		close(wfd);
		wfd = -1;
		if ( bad )
		{
			unlink(wpath);
			return -1;
		}
		if ( sellock() < 0 )		/* only NOW, with the I/O done */
		{
			unlink(wpath);
			return -1;
		}
		s->sq_seq++;			/* -> odd */
		oldfile = s->sq_file;
		s->sq_file = mypid;
		s->sq_len = wlen;
		s->sq_owner = wowner;
		s->sq_gen++;
		s->sq_seq++;			/* -> even */
		selunlock();
		if ( oldfile != mypid )
			unlinkpid(oldfile);
		return 0;
	}

	/* MEM: we have held the lock since hr_selopen and sq_seq is odd. */
	oldfile = s->sq_file;
	s->sq_file = 0;
	s->sq_len = bad ? 0L : wlen;
	s->sq_owner = bad ? -1 : wowner;
	s->sq_gen++;
	s->sq_seq++;				/* -> even */
	selunlock();
	unlinkpid(oldfile);
	return bad ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* reader -- no lock, and no idea which store it is reading           */
/* ------------------------------------------------------------------ */

/* Snapshot the header under the seqlock.  Returns 0, or -1 if a writer held it
 * odd for implausibly long (a writer that died mid-publish). */
static
selhdr(len, file, gen, owner)
long *len;
int *file, *gen, *owner;
{
	register HRSEL *s;
	register int n;
	int a, b;

	s = SEL;
	if ( s->sq_magic != HRSEL_MAGIC )	/* nobody has ever set a selection */
		return -1;
	for ( n = 0; n < READTRY; n++ )
	{
		a = s->sq_seq;
		if ( a & 1 )
			continue;
		*len = s->sq_len;
		*file = s->sq_file;
		*gen = s->sq_gen;
		*owner = s->sq_owner;
		b = s->sq_seq;
		if ( a == b )
			return 0;
	}
	return -1;
}

long
hr_sellen()
{
	long len;
	int file, gen, owner;

	if ( selhdr(&len, &file, &gen, &owner) < 0 )
		return 0L;
	return len < 0 ? 0L : len;
}

hr_selgen()
{
	long len;
	int file, gen, owner;

	if ( selhdr(&len, &file, &gen, &owner) < 0 )
		return -1;
	return gen;
}

hr_selowner()
{
	long len;
	int file, gen, owner;

	if ( selhdr(&len, &file, &gen, &owner) < 0 )
		return -1;
	return owner;
}

/* Copy at most `max' bytes of the selection starting at `off'.  Returns the byte
 * count (0 at end), or -1 if the selection changed underneath us -- in which case
 * the caller should restart from hr_sellen().  Works for either store: a file
 * body is read OUTSIDE any lock, so a paste never blocks a copy. */
hr_selread(off, buf, max)
long off;
char *buf;
{
	register HRSEL *s;
	register int i;
	register char *d;
	long len;
	int file, gen, owner, n, a, b, try;
	char path[24];

	if ( max <= 0 )
		return 0;
	if ( selhdr(&len, &file, &gen, &owner) < 0 )
		return -1;
	if ( off < 0 || off >= len )
		return 0;
	n = max;
	if ( off + (long)n > len )
		n = (int)(len - off);

	if ( file == 0 )
	{
		/* Tail store: re-check the seqlock ACROSS the copy, so a body being
		 * rewritten under us is retried rather than returned torn. */
		s = SEL;
		for ( try = 0; try < READTRY; try++ )
		{
			a = s->sq_seq;
			if ( a & 1 )
				continue;
			d = buf;
			for ( i = 0; i < n; i++ )
				*d++ = s->sq_data[(int)off + i];
			b = s->sq_seq;
			if ( a == b )
				return n;
		}
		return -1;
	}

	/* File store.  Keep the fd across chunks -- a paste streams the body in
	 * small pieces and re-opening per piece would be absurd -- but key it on
	 * the writer AND the generation so it is dropped as soon as the selection
	 * is replaced. */
	if ( rfd >= 0 && (rpid != file || rgen != gen) )
	{
		close(rfd);
		rfd = -1;
	}
	if ( rfd < 0 )
	{
		pidpath(path, HRSEL_PFX, file);
		if ( (rfd = open(path, 0)) < 0 )
			return -1;
		rpid = file;
		rgen = gen;
	}
	if ( lseek(rfd, off, 0) < 0 )
		return -1;
	n = read(rfd, buf, n);
	if ( n < 0 )
		return -1;
	/* Replaced while we were reading?  The bytes may straddle two selections. */
	if ( hr_selgen() != gen )
		return -1;
	return n;
}

/* ------------------------------------------------------------------ */
/* the CLIPBOARD -- the other store (shmem.h HRCLIP_PATH)             */
/* ------------------------------------------------------------------ */
/*
 * Same file plumbing as HRSEL_FILE above, pointed at a different name and with
 * everything the PRIMARY store needs and this one does not stripped out: no
 * header in the tail, so no lock, no seqlock, no generation, and nothing to
 * unlink when the next Copy lands.  Which is the whole point -- the two
 * mechanisms share code, not state, so a Copy cannot disturb a selection and a
 * selection cannot disturb the clipboard.
 *
 * A Copy streams into HRCLIP_NEW<pid> and is swapped into place by link/unlink
 * in hr_clipclose (see shmem.h for why that, and not a lock, is what makes a
 * paste safe against a simultaneous Copy).
 */

static int	copen;			/* 1 = a Copy is in progress           */
static int	cerr;			/* a write failed: publish nothing     */
static int	cwfd = -1;		/* the HRCLIP_NEW<pid> being written   */
static char	cwpath[24];

/* Reader fd, held across the chunks of one paste.  Unlike the selection reader
 * there is no generation to key it on -- and none is needed: this fd IS the
 * snapshot (the inode survives being unlinked from under us), so it is opened by
 * hr_cliplen and simply reused until the next hr_cliplen. */
static int	crfd = -1;

static
clipdrop()				/* forget the pinned snapshot */
{
	if ( crfd >= 0 )
		close(crfd);
	crfd = -1;
	return 0;
}

/* Begin a Copy.  Nothing is visible to a reader until hr_clipclose. */
hr_clipopen()
{
	if ( copen )
		return -1;
	if ( mypid == 0 )
		mypid = getpid();
	pidpath(cwpath, HRCLIP_NEW, mypid);
	if ( (cwfd = creat(cwpath, 0600)) < 0 )
		return -1;
	copen = 1;
	cerr = 0;
	return 0;
}

hr_clipwrite(buf, len)
char *buf;
{
	if ( !copen || len < 0 || cerr )
		return -1;
	if ( len && write(cwfd, buf, len) != len )
	{
		cerr = 1;
		return -1;
	}
	return 0;
}

/* Publish -- or, on a latched error, throw the whole Copy away and leave the
 * PREVIOUS clipboard in place.  That is the useful failure: a Copy that ran out
 * of disk should cost you the new text, not the text you already had. */
hr_clipclose()
{
	int bad;

	if ( !copen )
		return -1;
	bad = cerr;
	copen = 0;
	cerr = 0;
	close(cwfd);
	cwfd = -1;
	if ( bad )
	{
		unlink(cwpath);
		return -1;
	}
	clipdrop();			/* our own next paste must see the new one */
	unlink(HRCLIP_PATH);
	if ( link(cwpath, HRCLIP_PATH) < 0 )
	{
		unlink(cwpath);		/* the clipboard is now empty, not stale */
		return -1;
	}
	unlink(cwpath);			/* the link above is the file now */
	return 0;
}

/* Length of the clipboard, and the start of a paste: this is what OPENS the
 * snapshot that the following hr_clipread calls stream (see shmem.h).  0 for an
 * empty or absent clipboard -- never having copied anything is not an error. */
long
hr_cliplen()
{
	long n;

	clipdrop();
	if ( (crfd = open(HRCLIP_PATH, 0)) < 0 )
		return 0L;
	n = lseek(crfd, 0L, 2);
	if ( n < 0 )
	{
		clipdrop();
		return 0L;
	}
	return n;
}

/* Read a chunk of the pinned snapshot.  Returns the count (0 at end) or -1.
 * Opens the clipboard itself if hr_cliplen was not called first, which costs
 * only the guarantee that the whole paste came from one Copy. */
hr_clipread(off, buf, max)
long off;
char *buf;
{
	int n;

	if ( max <= 0 )
		return 0;
	if ( crfd < 0 && (crfd = open(HRCLIP_PATH, 0)) < 0 )
		return -1;
	if ( lseek(crfd, off, 0) < 0 )
		return -1;
	n = read(crfd, buf, max);
	return n < 0 ? -1 : n;
}

/* What the window menu's "Copy" does: take whatever the PRIMARY selection holds
 * right now and put it in the clipboard.  It reads through hr_selread, so the
 * selection may be in either of ITS two stores, and it touches no selection
 * state at all -- no lock, no ownership, no E_SELCLEAR -- so the highlight the
 * user is looking at is still there afterwards, which is the difference between
 * this and a middle-click.  Returns 0, or -1 if there was nothing to copy or the
 * selection was replaced mid-copy (in which case the old clipboard stands). */
hr_clipfromsel()
{
	char b[128];
	long off, len;
	int n;

	len = hr_sellen();
	if ( len <= 0 )
		return -1;
	if ( hr_clipopen() < 0 )
		return -1;
	for ( off = 0; off < len; off += n )
	{
		n = hr_selread(off, b, sizeof(b));
		if ( n < 0 )
			cerr = 1;		/* changed under us: discard the lot */
		if ( n <= 0 )
			break;
		if ( hr_clipwrite(b, n) < 0 )
			break;
	}
	return hr_clipclose();
}

/* ------------------------------------------------------------------ */
/* server: initialise the store                                       */
/* ------------------------------------------------------------------ */

/* The VRAM tail is uninitialised RAM at power-on, so the header MUST be stamped
 * before anyone reads it or a garbage sq_len/sq_file is taken at face value.
 * zview calls this once at start-up, next to loadfont -- and hr_selopen calls it
 * too when it finds no magic, so the store also works on a boot where zview never
 * ran at all (which is how hrclip is testable on the plain serial emulator).
 * Any file the CURRENT header names is dropped; strays from a writer that died
 * between creat and publish are not hunted down here -- /etc/rc sweeps /tmp,
 * dot files included, once the partition is mounted, which is the only way they
 * outlive a session. */
hr_selinit()
{
	register HRSEL *s;

	s = SEL;
	if ( s->sq_magic == HRSEL_MAGIC )	/* a real predecessor, not garbage */
		unlinkpid(s->sq_file);
	s->sq_magic = HRSEL_MAGIC;
	s->sq_seq = 0;
	s->sq_gen = 0;
	s->sq_owner = -1;
	s->sq_file = 0;
	s->sq_len = 0;
	*SELLOCK = 0;
	SELPID = 0;
	SELTIME = 0;
	return 0;
}

/* Is there anything behind the tail at all?  On a machine with no hi-res card
 * segments 0x3A/0x3B have no responder: stores are dropped and loads float to
 * 0xFF (both emulators model this exactly, and it is how the firmware decides to
 * fall back to the serial console).  Every store here would then silently go
 * nowhere, so probe it the way the boot ROM probes the framebuffer -- write and
 * read back -- and let the caller say so out loud instead of appearing to work.
 *
 * Probes SHM_SELPROBE, a scratch word in the spare tail past the lock words, so
 * it disturbs no live state and is safe to call at any time.  The value goes
 * through a pointer the compiler cannot see through, so the store cannot be
 * optimised away against the load (this K&R compiler has no `volatile'). */
hr_selok()
{
	register short *p;

	p = (short *)(HRTAIL + SHM_SELPROBE);
	*p = 0x1234;
	if ( *p != 0x1234 )
		return 0;
	*p = 0x4321;
	return *p == 0x4321;
}

