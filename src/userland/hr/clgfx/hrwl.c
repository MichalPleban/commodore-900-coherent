/*
 * hrwl.c - read the server-published window list (shmem.h SHM_WINLIST).
 *
 * The server mirrors its private wins[] bookkeeping into the shared VRAM
 * tail (zview publish_wins) so any client -- a taskbar, a monitor, a
 * switcher -- can enumerate the desktop without a round trip to the server.
 * Readers take no lock: they retry on the seqlock (odd = the server is
 * rewriting), the same idiom as the HRSURF clip descriptors.  The retry is
 * BOUNDED: a server that died mid-write leaves wl_seq odd forever, and a
 * window list is display-only data -- after enough tries we hand back the
 * possibly-torn copy rather than hang the caller (the title bytes are
 * NUL-capped below so even a torn title is a safe string).
 */
#include "shmem.h"

#define HRWL_TRIES	200	/* a publish is a few hundred stores: generous */

/* Snapshot the table into out[HRWL_N] (slot index = window id).  Returns the
 * number of live windows, or -1 when no server session owns the tail (magic
 * clear: zview not running, or gone).  A poller that only wants "did anything
 * change" should watch hr_winseq() and skip the copy. */
int
hr_winlist(out)
register HRWIN *out;
{
	register HRWIN *pw;
	register HRWLIST *wl;
	int w, i, s, n, tries;

	if ( hr_glob()->magic != HR_MAGIC )
		return -1;
	wl = hr_wlist();
	for ( tries = 0; ; tries++ )
	{
		s = wl->wl_seq;
		if ( (s & 1) == 0 || tries >= HRWL_TRIES )
		{
			n = 0;
			for ( w = 0; w < HRWL_N; w++ )
			{
				pw = &wl->wl_win[w];
				out[w].ww_used = pw->ww_used;
				out[w].ww_pid = pw->ww_pid;
				out[w].ww_min = pw->ww_min;
				for ( i = 0; i < 24; i++ )
					out[w].ww_title[i] = pw->ww_title[i];
				out[w].ww_title[23] = 0;
				if ( out[w].ww_used )
					n++;
			}
			if ( wl->wl_seq == s || tries >= HRWL_TRIES )
				return n;
		}
	}
}

/* The list's generation word: it moves exactly when the list changes, so a
 * periodic poller compares this against what it last saw before paying for a
 * full hr_winlist() copy.  Meaningless when no server is up -- gate on
 * hr_winlist()'s -1 (or hr_glob()->magic) first. */
int
hr_winseq()
{
	return hr_wlist()->wl_seq;
}
