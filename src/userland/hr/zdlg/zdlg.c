/*
 * zdlg.c - dialog demo / test client (the widget kit's reference consumer).
 *
 * A small window; a left click in its content (or the window menu's
 * "Settings") opens a modal settings dialog exercising every widget:
 * label, text field, checkboxes, a radio pair, OK / Cancel.  The outcome is
 * appended to /dlgtest.out and sync()ed, so a scripted emulator run can
 * assert on it with disk.py after the fact (CLAUDE.md: isolated testing).
 */
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"

HRAPP	me = { "DlgTest", "", 240, 100, 0, 0, 0, HRM_SETTINGS };

char	nmbuf[24] = "coherent";

HRWIDGET wg[] = {
    { DW_LABEL,   12,  10,   0,  0, "Name:" },
    { DW_TEXT,    70,   4, 200, 22, (char *)0, 0, 0, nmbuf, sizeof(nmbuf) },
    { DW_CHECK,   12,  44,   0,  0, "Alpha", 1 },
    { DW_CHECK,   12,  66,   0,  0, "Beta" },
    { DW_RADIO,  160,  44,   0,  0, "One", 1, 1 },
    { DW_RADIO,  160,  66,   0,  0, "Two", 0, 1 },
    { DW_BUTTON,  50, 112,  80, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170, 112,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define NWG	(sizeof(wg) / sizeof(wg[0]))

repaint()
{
	cl_refresh();
	if ( !cl_mapped() || cl_frozen() )
		return;
	cl_begin();
	cl_fillrect(0, 0, cl_cw(), cl_ch(), 1);
	cl_ptext(SHM_FUI, 8, 8, "Click here (or menu");
	cl_ptext(SHM_FUI, 8, 26, "Settings) to open");
	cl_ptext(SHM_FUI, 8, 44, "the dialog.");
	cl_end();
}

/* Log one dialog outcome (button index + widget state) for the test harness. */
logresult(r)
{
	char line[80];
	int fd;

	sprintf(line, "r=%d name=%s alpha=%d beta=%d radio=%s\n",
		r, nmbuf, wg[2].dw_val, wg[3].dw_val,
		wg[4].dw_val ? "one" : "two");
	if ( (fd = open("/dlgtest.out", 1)) < 0 ||
	     lseek(fd, 0L, 2) < 0 )		/* append; create if absent */
	{
		if ( fd >= 0 )
			close(fd);
		fd = creat("/dlgtest.out", 0644);
	}
	if ( fd >= 0 )
	{
		write(fd, line, strlen(line));
		close(fd);
	}
	sync();
}

dosettings()
{
	int w, h, r;

	w = 300;
	h = 150;
	r = hr_dlgopen(&w, &h);
	if ( r < 0 )
	{
		logresult(r == -2 ? -2 : -9);	/* -9 = refused/failed */
		if ( r == -2 )
			exit(0);
		return;
	}
	hr_dlgdraw(wg, NWG);
	r = hr_dlgrun(wg, NWG);
	hr_dlgclose();
	logresult(r);
	if ( r == -1 )				/* window died mid-dialog */
		exit(0);
}

main(argc, argv)
char **argv;
{
	WMSG e;

	if ( hr_open(&me, &argc, argv) < 0 )
	{
		write(2, "zdlg: no window server\n", 23);
		exit(1);
	}
	repaint();
	for (;;)
	{
		hr_evwait(hr_wid());
		while ( hr_evget(hr_wid(), (short *)&e) )
		{
			if ( e.wm_type == E_EXPOSE || e.wm_type == E_RESIZE )
				repaint();
			else if ( e.wm_type == E_QUIT )
				exit(0);
			else if ( e.wm_type == E_MENU &&
				  e.wm_arg[0] == HRM_SETTINGS )
				dosettings();
			else if ( e.wm_type == E_BUTTON &&
				  (e.wm_arg[2] & EB_LEFT) )
				dosettings();
		}
		if ( hr_evover(hr_wid()) )
			repaint();
	}
}
