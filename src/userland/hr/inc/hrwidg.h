/*
 * hrwidg.h - dock-widget client support (hrwidg.c).
 *
 * A WIDGET is a windowless GUI process the dock (zdock) forks to draw live
 * content inside one cell of the dock bar.  It never connects to the server:
 * zdock passes its own window id, the cell rect (dock-content coords) and its
 * pid on the command line as
 *
 *	-W wid,x0,y0,x1,y1,dockpid
 *
 * and hr_wopen() turns that into a clgfx sub-surface (cl_subinit), after which
 * the ordinary cl_* primitives draw into the cell, clipped by the dock
 * window's server-published visible rects.  hr_wlive() is the widget's
 * liveness gate: it goes 0 when the dock window is gone OR its wid was reused
 * by another client (pid check), so the widget exits instead of scribbling.
 */
#ifndef HRWIDG_H
#define HRWIDG_H

extern int	hr_wopen();	/* hr_wopen(&argc, argv): parse -W, close the
				   command pipe, cl_subinit; 0 ok / -1 no -W  */
extern int	hr_wlive();	/* 1 while the dock still owns the host window */
extern int	hr_wdockwid();	/* the host (dock) window id, after hr_wopen   */

#endif /* HRWIDG_H */
