/*
 * zmaze.c - Wolfenstein-style raycast maze, as a ZView window.
 *
 * The front end of the zmcore.c raycaster: a fixed 320x200 window whose
 * content is one first-person frame.  The core renders into frame[] and
 * this file presents it with cl_blit() -- so an E_EXPOSE never re-renders,
 * it just re-blits the persistent image, and a frame only ever renders in
 * response to a key.  That key-driven cadence is also what makes the
 * unclipped ldir fast path inside cl_blit the common case: the window has
 * focus when the player moves, so it is normally frontmost and fully
 * visible; when it is not (or its screen position is not word-aligned),
 * cl_blit falls back to the clipped engine bitblt per visible rect.
 *
 *   w/s or Up/Down     forward/back
 *   a/d or Left/Right  turn left/right
 *   q    (or the window menu's Quit) leaves the game
 *
 * A middle-click (E_PASTE) replays the PRIMARY selection as keystrokes, so
 * a recorded walk pastes straight into the maze -- and the mouse-only test
 * rig can drive the game without the keyboard.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"

#define W	320		/* viewport = content size */
#define H	200

extern int	frame[];	/* zmcore.c: the rendered 1bpp image */

HRAPP	me = { "Maze", "maze.icn", W, H, 0, 0, 0, HRM_HELP };

/* Help (menu entry or F11), zedit-style: a modal hrdlg card. */
static HRWIDGET hwg[] = {
    { DW_LABEL, 12,  12, 0, 0, "Find your way around the maze." },
    { DW_LABEL, 12,  40, 0, 0, "w / Up      walk forward" },
    { DW_LABEL, 12,  60, 0, 0, "s / Down    walk backward" },
    { DW_LABEL, 12,  80, 0, 0, "a / Left    turn left" },
    { DW_LABEL, 12, 100, 0, 0, "d / Right   turn right" },
    { DW_LABEL, 12, 128, 0, 0, "Middle-click replays the PRIMARY" },
    { DW_LABEL, 12, 148, 0, 0, "selection as keys: paste a walk." },
    { DW_LABEL, 12, 176, 0, 0, "Walls fade darker with distance;" },
    { DW_LABEL, 12, 196, 0, 0, "a full-screen checker is a wall" },
    { DW_LABEL, 12, 216, 0, 0, "point-blank -- turn away.  q quits." },
    { DW_BUTTON, 145, 244, 70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define NHWG	(sizeof(hwg) / sizeof(hwg[0]))

static
dohelp()
{
	int w, h, r;

	w = 360;
	h = 288;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(hwg, NHWG);
	r = hr_dlgrun(hwg, NHWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return 0;
}

int	mywid;

static
present()
{
	cl_blit(0, 0, W, H, frame, W/16);
	cl_snapclip();
	return 0;
}

/* Replay the PRIMARY selection as game keys; returns 1 if the view moved. */
static
pastekeys()
{
	char b[64];
	long off, len;
	int n, i, moved;

	moved = 0;
	len = hr_sellen();
	for ( off = 0; off < len; off += n )
	{
		n = hr_selread(off, b, sizeof(b));
		if ( n <= 0 )
			break;
		for ( i = 0; i < n; i++ )
			moved |= dokey(b[i] & 0xff);
	}
	return moved;
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int need, dirty;

	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */

	inittab();
	render();

	need = 1;
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		present();
		need = 0;
	}
	for (;;)
	{
		hr_evwait(mywid);
		dirty = 0;
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
			case E_RESIZE:
				need = 1;
				break;

			case E_KEY:
				switch ( e.wm_arg[0] & 0xff )
				{
				case 'q':
					hr_bye();
					exit(0);
				case HRK_HELP:		/* F11 */
					dohelp();
					break;
				default:
					dirty |= dokey(e.wm_arg[0] & 0xff);
				}
				break;

			case E_MENU:
				if ( e.wm_arg[0] == HRM_HELP )
					dohelp();
				break;

			case E_PASTE:	/* middle-click: replay the selection */
				dirty |= pastekeys();
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )
			need = 1;
		if ( dirty )
		{
			render();
			need = 1;
		}
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )	/* a blit was lost against a freeze */
				need = 1;
			if ( need )
			{
				present();
				need = 0;
			}
		}
	}
}
