/*
 * clgfx.h - hrgui client-side direct-render draw library (GUI.md Model A, 2.4/2.9).
 *
 * A direct-render client draws its OWN content straight into the framebuffer
 * (segments 0x3A/0x3B), clipped to the visible-region list the server publishes
 * for its window in the shared VRAM tail (shmem.h).  No pixels and no per-glyph
 * traffic cross IPC -- only control/events stay on the pipes.  Coordinates are
 * WINDOW-RELATIVE (0,0 = content top-left); clgfx adds the physical origin and
 * clips (GUI.md 2.10).  Glyphs are blitted a whole row at a time via the engine
 * bitblt (never per-pixel); fonts come from the shared tail (no relink).
 *
 * Usage per repaint batch:
 *     cl_begin();                         -- refresh clip descriptor + hide cursor
 *     cl_text(SHM_FTERM, col,row,s, cw,ch);
 *     cl_erase(col,row,ncol,nrow, cw,ch);
 *     ... or cl_line/cl_point/cl_fillrect for graphics ...
 *     cl_end();                           -- restore cursor
 */
#ifndef CLGFX_H
#define CLGFX_H

extern int	cl_init();	/* cl_init(wid): one-time setup for window wid    */
extern int	cl_begin();	/* refresh descriptor (seqlock) + bracket cursor  */
extern int	cl_end();	/* end batch, show cursor                         */
extern int	cl_mapped();	/* 1 if the window is live & has a clip descriptor */
extern int	cl_cw();	/* content width  in px (from descriptor)         */
extern int	cl_ch();	/* content height in px                           */
extern int	cl_fullyvis();	/* 1 if content is a single unclipped rect        */
extern int	cl_frozen();	/* 1 while a server menu/overlay is up: skip drawing */
extern int	cl_refresh();	/* re-read the clip descriptor (seqlock)          */
extern int	cl_gen();	/* clip-descriptor generation (changes on hide/show/raise/resize) */
extern int	cl_snapclip();	/* snapshot the clip after a full repaint          */
extern int	cl_uncovered();	/* 1 if the clip now shows area the cl_snapclip
				   snapshot did not (covered-MORE returns 0)      */
extern int	cl_dropped();	/* read+clear: a primitive was skipped while
				   frozen/unmapped -> one full repaint is owed    */
extern int	cl_dopen();	/* target the DIALOG surface (hrdlg.c uses these) */
extern int	cl_dclose();	/* back to the window surface                     */

extern int	cl_text();	/* cl_text(fslot, col,row, s, cellw,cellh)        */
extern int	cl_ptext();	/* cl_ptext(fslot, cx,cy, s): pixel-positioned    */
extern int	cl_ptextt();	/* like cl_ptext but transparent: ink only, cell
				   background untouched (overlay double-strike)   */
extern int	cl_erase();	/* cl_erase(col,row, ncol,nrow, cellw,cellh) white */
extern int	cl_fillrect();	/* cl_fillrect(cx0,cy0,cx1,cy1, val) content px;
				   val 0=black 1=white 2=invert 3=50% gray       */
extern int	cl_point();	/* cl_point(cx,cy, val)                           */
extern int	cl_line();	/* cl_line(cx0,cy0,cx1,cy1, val)                  */

#endif /* CLGFX_H */
