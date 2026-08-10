/*
 * hrdlg.h - hrgui modal dialogs: shared chrome metrics + the client widget kit.
 *
 * Two consumers, one look:
 *   - zview's srvdialog() (its OWN confirmations -- quit-the-desktop, per-app
 *     quit) draws message + buttons with the DLG_* metrics below;
 *   - the client widget library (clgfx/hrdlg.c) draws the same chrome inside a
 *     client dialog overlay (wire.h C_DLGOPEN), so a server confirmation and an
 *     application dialog are indistinguishable on screen.
 *
 * A client dialog is a MODAL OVERLAY CANVAS, not a window: hr_dlgopen() asks
 * the server to save the pixels under a centred box (untitled card, 1px
 * border, window-style drop shadow) and publish the interior as this client's
 * drawable surface (shmem.h SHM_DLGSURF); every cl_* primitive then targets
 * that canvas, so an application can compose any content it likes (settings
 * panels, file lists) and lay the widgets below over it.  hr_dlgrun() runs
 * the modal event loop; hr_dlgclose() restores the screen.  All coordinates
 * are DIALOG-INTERIOR pixels, font is SHM_FUI (9x16).
 *
 * Usage:
 *     static char fname[32];
 *     static HRWIDGET wg[] = {
 *         { DW_LABEL,  12, 12,   0,  0, "File name:" },
 *         { DW_TEXT,   12, 34, 200, 22, 0, 0, 0, fname, sizeof(fname) },
 *         { DW_BUTTON, 60, 70,  70, DLG_BTNH, "OK",     0,0,0,0, DWF_DEF|DWF_END },
 *         { DW_BUTTON,150, 70,  70, DLG_BTNH, "Cancel", 0,0,0,0, DWF_CANCEL|DWF_END },
 *     };
 *     int w = 240, h = 104;
 *     if ( hr_dlgopen(&w, &h) == 0 ) {
 *         hr_dlgdraw(wg, 4);
 *         if ( hr_dlgrun(wg, 4) == 2 )    -- OK --
 *             use(fname);
 *         hr_dlgclose();
 *     }
 */
#ifndef HRDLG_H
#define HRDLG_H

/* ---- chrome metrics (shared with zview's srvdialog) ---------------------- */
#define DLG_MARG	12	/* box margin around everything             */
#define DLG_GAPY	12	/* message block <-> button row             */
#define DLG_GAPX	16	/* between buttons                          */
#define DLG_BTNH	24	/* button height: 16 px FUI text + 2*4 pad  */
#define DLG_BTNPAD	8	/* label side padding inside a button       */
#define DLG_BSHAD	2	/* button drop shadow: a mini WD_SHADOW, so */
				/* a button sits raised off the card the    */
				/* same way the card sits off the desktop.  */
				/* Lay buttons out with this much clear to  */
				/* their right and below (DLG_GAPX and the  */
				/* bottom DLG_MARG already allow for it).   */
#define DLG_CHK		13	/* checkbox / radio glyph square            */
#define DLG_THPAD	3	/* text-field inner pad                     */

/* ---- widgets ------------------------------------------------------------- */
#define DW_LABEL	0	/* static text                              */
#define DW_BUTTON	1	/* push button (arm/track, commit on release) */
#define DW_TEXT		2	/* single-line edit field (first one gets   */
				/* the focus; click or Tab moves it)        */
#define DW_CHECK	3	/* on/off toggle                            */
#define DW_RADIO	4	/* one-of-group (dw_grp)                    */

#define DWF_DEF		0x01	/* default button: double border, Enter fires */
#define DWF_CANCEL	0x02	/* Esc fires it                             */
#define DWF_END		0x04	/* activating it ends hr_dlgrun (returns index) */

typedef struct {
	int	dw_type;	/* DW_*                                     */
	int	dw_x, dw_y;	/* top-left, dialog-interior px             */
	int	dw_w, dw_h;	/* size (DW_LABEL may leave 0: text extent) */
	char	*dw_label;	/* label / button text (0 for DW_TEXT)      */
	int	dw_val;		/* DW_CHECK/DW_RADIO: 0/1                   */
	int	dw_grp;		/* DW_RADIO: group id (exclusive within)    */
	char	*dw_buf;	/* DW_TEXT: caller's NUL-terminated buffer  */
	int	dw_len;		/* DW_TEXT: sizeof that buffer              */
	int	dw_flags;	/* DWF_*                                    */
} HRWIDGET;

extern int	hr_dlgopen();	/* hr_dlgopen(&w, &h) -> 0 ok, -1 failed/   */
				/* refused, -2 E_QUIT (exit); may block     */
				/* behind a dialog already up               */
extern int	hr_dlgdraw();	/* hr_dlgdraw(wg, n): (re)draw every widget */
extern int	hr_dlgrun();	/* hr_dlgrun(wg, n) -> DWF_END widget index, */
				/* or -1 on E_QUIT (clean up and exit)      */
extern int	hr_dlgclose();	/* restore the screen, leave dialog mode    */

#endif /* HRDLG_H */
