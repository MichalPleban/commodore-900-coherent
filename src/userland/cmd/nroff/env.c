/*
 * Nroff/Troff.
 * Enviroments.
 */
#include <stdio.h>
#include "roff.h"
#include "code.h"
#include "env.h"
#include "esc.h"

/*
 * Save enviroment parameters that might be destroyed when processing
 * a single line of text and initialise for a new line.
 */
/*
newenvr()
{
	register ENV *ep;

	ep = nalloc(sizeof (ENV));
	copystr(ep, &env, sizeof (ENV), 1);
	setline();
	return(ep);
}
*/

/*
 * Restore enviroment parameters that were previously saved with
 * the function `newenvr'.
 */
/*
resenvr(ep)
register ENV *ep;
{
	copystr(&env, ep, sizeof (ENV), 1);
	nfree(ep);
}
*/

/*
 * Initialise the current enviroment.
 */
setenvr()
{
	register int inc, i, n;

	setline();
	preexls = 0;
	posexls = 0;
	setfont("R", 1);
	setpsze(unit(10*SMPOIN, SDPOIN));
	devvlsp(newpsz);
	tab[0].t_pos = 0;
	tab[0].t_jus = LJUS;
	inc = n = unit(8*SMINCH, 10*SDINCH);
	for (i=1; i<TABSIZE-1; i++) {
		env.e_tab[i].t_pos = n;
		env.e_tab[i].t_jus = LJUS;
		n += inc;
	}
	tab[TABSIZE-1].t_pos = 0;
	tab[TABSIZE-1].t_jus = NJUS;
	lln = unit(13*SMINCH, 2*SDINCH);
	ind = 0;
	tin = 0;
	tif = 0;
	fil = 1;
	adm = 1;
	adj = FJUS;
	cec = 0;
	ulc = 0;
	uft[0] = 'I';
	uft[1] = '\0';
	mws = unit(SMEMSP, SDEMSP);
	lsp = 1;
	hyp = 1;
	tln = lln;
	mar = 0;
	csz = 0;
	lgm = 0;
	lnn = 0;
	lmn = 0;
	lns = 0;
	lni = 0;
	nnc = 0;
	inpltrc = 0;
	inptrap[0] = '\0';
	oldfon[0] = fon[0];
	oldfon[1] = fon[1];
	oldpsz = psz;
	oldvls = vls;
	oldlln = lln;
	oldind = ind;
	oldmws = mws;
	oldlsp = lsp;
	oldtln = tln;
	oldmar = mar;
	ccc = '.';
	nbc = '\'';
	tbc = '\0';
	ldc = '.';
	hic = EHYP;
	tpc = '%';
	mrc = '\0';
}

/*
 * Given a font name, change font.
 */
setfont(name, setflag)
char name[2];
{
	register FTB *fp;

	if ((name[0] >= '1') && (name[0] <='8'))
		name[0] = mapfont[name[0] - '0' - 1];

	if (name[0]=='P' && name[1]=='\0') {
		name[0] = oldfon[0];
		name[1] = oldfon[1];
	}
	fp = fontab;
	while (fp->f_name[0]!=name[0] || fp->f_name[1]!=name[1]) {
		if (fp++->f_name[0] == '\0') {
			printe("Cannot find font");
			return;
		}
	}
	devfont(fp->f_font, fp->f_bold, fp->f_ital);
	if (setflag) {
		oldfon[0] = fon[0];
		oldfon[1] = fon[1];
		fon[0] = name[0];
		fon[1] = name[1];
	}
}

/*
 * Set the current pointsize.
 */
setpsze(n)
{
	devpsze(n);
}
