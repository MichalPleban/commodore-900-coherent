/*
 * hrapp.h - hrgui client start-up: how a GUI application gets its window.
 *
 * A ZView application is a plain program.  It is exec'd with its OWN argv (the
 * server appends nothing) and takes no window id, size or cell metrics on the
 * command line; it declares what it wants by filling an HRAPP and calling
 * hr_open() as its first act:
 *
 *	#include "wire.h"		-- HRF_* live there (the wire flags)
 *	#include "shmem.h"
 *	#include "clgfx.h"
 *	#include "hrapp.h"
 *
 *	HRAPP me = { "Clock", "clock.icn", 240, 240, HRF_STRETCH };
 *
 *	main(argc, argv)
 *	char **argv;
 *	{
 *		if ( hr_open(&me, &argc, argv) < 0 )
 *			exit(1);		-- not running under zview
 *		... me.ha_w / me.ha_h are now the GRANTED content size ...
 *	}
 *
 * hr_open() sends the declaration to the server (C_CONNECT), waits for the
 * window to exist (E_CONNECTED), runs cl_init() so clgfx can draw, and writes
 * the granted content size back into the HRAPP -- the server may clamp what was
 * asked for to fit the screen, so a client must always use what it gets back
 * rather than what it asked for.  Everything else (events, drawing) is as before.
 *
 * Menu entries.  A window's only menu is the server's right-button pop-up, so an
 * application that wants commands of its own asks for them in ha_menu, as a set
 * of HRM_* bits (wire.h):
 *
 *	HRAPP me = { "Edit", "edit.icn", 480, 300, HRF_STRETCH, 0, 0,
 *		     HRM_OPEN | HRM_SAVE | HRM_CUT | HRM_COPY | HRM_PASTE };
 *
 * The entries appear at the TOP of that window's menu, in the fixed HRM_ order,
 * followed by a divider and then the usual window operations (Move, Stretch,
 * Front, Back, Hide, Quit).  Choosing one delivers an E_MENU event whose arg0 is
 * the chosen bit, so the client's event loop dispatches on it like any other
 * event -- nothing else changes, and an app that sets ha_menu to 0 (i.e. does
 * not mention it at all) gets exactly the menu it had before.
 *
 * Fonts are NOT passed in either: cell metrics come from the shared VRAM tail,
 * e.g. hr_font(SHM_FTERM)->cellw / ->cellh (shmem.h), which a client may read
 * before hr_open() -- handy for asking for a size in whole character cells.
 *
 * Options recognised globally (removed from argv, argc updated), so every GUI
 * app accepts them without writing any code:
 *	-T <string>	title: overrides ha_title
 *	-I <file.icn>	desktop icon: overrides ha_icon
 *	-S <W>x<H>	content size: overrides ha_w x ha_h, but ONLY for an
 *			app that declared HRF_STRETCH -- a fixed-layout window
 *			sized from the outside would simply be wrong, so the
 *			option is ignored (not an error) for one
 *	-P <X>,<Y>	where to put the window: the top-left of its FRAME, in
 *			screen pixels (the server still clamps it on screen)
 *	-H		open minimised: the window goes straight to a desktop
 *			icon and appears when the user restores it
 *
 * These are what makes a start-up script possible: /usr/hr/etc/rc is an ordinary
 * shell script, so the desktop layout is written there
 *
 *	/usr/hr/bin/zterm -P 48,40 &
 *	/usr/hr/bin/zclock -P 470,150 -H &
 *
 * rather than compiled into the server.  An app run that way is NOT a child of
 * the server (see wire.h: it makes its own event pipe), which is invisible here
 * -- hr_open() sorts it out either way.  Note the & : the apps do not exit.
 */
#ifndef HRAPP_H
#define HRAPP_H

/* An application's declaration of the window it wants.  ha_w/ha_h are updated
 * in place by hr_open() to the size actually granted.  ha_x/ha_y are usually
 * left alone: they matter only when ha_flags has HRF_POS, which is what -P
 * sets, and an app that does not care lets the server place the window. */
typedef struct {
	char	*ha_title;	/* window title; also the instance base name    */
	char	*ha_icon;	/* .icn under /usr/hr/icons; NULL/"" = default; */
				/* a name that is not installed also falls back */
	int	ha_w, ha_h;	/* desired -> granted content size, px          */
	int	ha_flags;	/* HRF_* (wire.h); HRF_STRETCH = resizable      */
	int	ha_x, ha_y;	/* wanted frame origin, px, if HRF_POS          */
	int	ha_menu;	/* HRM_* (wire.h): our window-menu entries, 0 = */
				/* none.  See "Menu entries" below.             */
} HRAPP;

extern int	hr_open();	/* hr_open(&app, &argc, argv) -> wid, or -1    */
extern int	hr_wid();	/* our window id (-1 before hr_open succeeds)  */
extern int	hr_bye();	/* tell the server to reap our window          */
extern int	hr_cmd();	/* hr_cmd(C_*): send a bare command record     */

#endif /* HRAPP_H */
