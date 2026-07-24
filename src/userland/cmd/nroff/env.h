/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Enviroment header file.
 */
#define llinptr	env.e_llinptr		/* Last line pointer */
#define nlinptr	env.e_nlinptr		/* New line pointer */
#define tlinptr	env.e_tlinptr		/* Tab line pointer */
#define llinsiz	env.e_llinsiz		/* Last line size */
#define nlinsiz	env.e_nlinsiz		/* New line size */
#define tlinsiz	env.e_tlinsiz		/* Tab line size */
#define llindir	env.e_llindir		/* Last directive count */
#define nlindir	env.e_nlindir		/* Current directive count */
#define ctabptr	env.e_ctabptr		/* Current tab pointer */
#define ltabchr	env.e_ltabchr		/* Last tab character */
#define basline	env.e_basline		/* Height above baseline */
#define preexls	env.e_preexls		/* Pre extra line space */
#define posexls	env.e_posexls		/* Post extra line space */
#define defwidt	env.e_defwidt		/* Default font width table */
#define fonwidt	env.e_fonwidt		/* Current font width table */
#define fontype	env.e_fontype		/* Current font */
#define itlmode	env.e_itlmode		/* Italic */
#define bolmode	env.e_bolmode		/* Boldface */
#define tab	env.e_tab		/* Table of tabs */
#define fon	env.e_fon		/* Font name */
#define psz	env.e_psz		/* Pointsize */
#define vls	env.e_vls		/* Vertical line spacing */
#define lln	env.e_lln		/* Line length */
#define ind	env.e_ind		/* Indent */
#define tin	env.e_tin		/* Temporary indent */
#define tif	env.e_tif		/* Temporary indent flag */
#define fil	env.e_fil		/* Fill mode */
#define adm	env.e_adm		/* Adjust mode */
#define adj	env.e_adj		/* Adjust justification type */
#define cec	env.e_cec		/* Count for centre command */
#define ulc	env.e_ulc		/* Count for underline command */
#define uft	env.e_uft		/* Underline font */
#define ufp	env.e_ufp		/* Previous font (from underline) */
#define mws	env.e_mws		/* Minimum word spacing */
#define lsp	env.e_lsp		/* Line spacing */
#define hyp	env.e_hyp		/* Hyphenation mode */
#define tln	env.e_tln		/* Title length */
#define mar	env.e_mar		/* Margin seperation */
#define csz	env.e_csz		/* Constant character size */
#define lgm	env.e_lgm		/* Ligature mode */
#define lnn	env.e_lnn		/* Current line number */
#define lmn	env.e_lmn		/* Line number multiple */
#define lns	env.e_lns		/* Line number seperation */
#define lni	env.e_lni		/* Line number indent */
#define nnc	env.e_nnc		/* Count for no number command */
#define	inpltrc	env.e_inpltrc		/* Input line trap count */
#define	inptrap	env.e_inptrap		/* Input line trap */
#define oldfon	env.e_oldfon		/* Last value of fon */
#define oldpsz	env.e_oldpsz		/* Last value of psz */
#define oldvls	env.e_oldvls		/* Last value of vls */
#define oldlln	env.e_oldlln		/* Last value of lln */
#define oldind	env.e_oldind		/* Last value of ind */
#define oldmws	env.e_oldmws		/* Last value of mws */
#define oldlsp	env.e_oldlsp		/* Last value of lsp */
#define oldtln	env.e_oldtln		/* Last value of tln */
#define oldmar	env.e_oldmar		/* Last value of mar */
#define ccc	env.e_ccc		/* Current control character */
#define nbc	env.e_nbc		/* No break character */
#define tbc	env.e_tbc		/* Tab character */
#define ldc	env.e_ldc		/* Leader character */
#define hic	env.e_hic		/* Hyphenation indicator character */
#define tpc	env.e_tpc		/* Page character in title */
#define mrc	env.e_mrc		/* Margin character */
#define linebuf	env.e_linebuf		/* Line buffer */
#define wordbuf	env.e_wordbuf		/* Word buffer */
#define	newbold	env.e_newbold		/* New value of bolmode */
#define	newital	env.e_newital		/* New value of itlmode */
#define	newfont	env.e_newfont		/* New value of fontype */
#define	newpsz	env.e_newpsz		/* New value of psz */

/*
 * Tab stop entry.
 */
typedef	struct {
	int	t_pos;			/* Tab position */
	int	t_jus;			/* Type of justification */
} TAB;

/*
 * Enviroments
 */
typedef	struct {
	CODE	*e_llinptr;		/* Last line pointer */
	CODE	*e_nlinptr;		/* New line pointer */
	CODE	*e_tlinptr;		/* Tab line pointer */
	int	e_llinsiz;		/* Last line size */
	int	e_nlinsiz;		/* New line size */
	int	e_tlinsiz;		/* Tab line size */
	int	e_llindir;		/* Last directive count */
	int	e_nlindir;		/* New directive count */
	TAB	*e_ctabptr;		/* Current tab pointer */
	char	e_ltabchr;		/* Last tab character */
	int	e_basline;		/* Height above baseline */
	int	e_preexls;		/* Pre extra line space */
	int	e_posexls;		/* Post extra line space */
	char	*e_defwidt;		/* Default font width table */
	char	*e_fonwidt;		/* Current font width table */
	int	e_fontype;		/* Current font */
	int	e_itlmode;		/* Italic */
	int	e_bolmode;		/* Boldface */
	TAB	e_tab[TABSIZE];		/* Table of tabs */
	char	e_fon[2];		/* Font name */
	int	e_psz;			/* Pointsize */
	int	e_vls;			/* Vertical line spacing */
	int	e_lln;			/* Line length */
	int	e_ind;			/* Indent */
	int	e_tin;			/* Temporary indent */
	int	e_tif;			/* Temporary indent flag */
	int	e_fil;			/* Fill mode */
	int	e_adm;			/* Adjust mode */
	int	e_adj;			/* Adjust justification type */
	int	e_cec;			/* Count for centre command */
	int	e_ulc;			/* Count for underline command */
	char	e_uft[2];		/* Underline font */
	char	e_ufp[2];		/* Previous font (from underline) */
	int	e_mws;			/* Minimum word spacing */
	int	e_lsp;			/* Line spacing */
	int	e_hyp;			/* Hyphenation mode */
	int	e_tln;			/* Title length */
	int	e_mar;			/* Margin seperation */
	int	e_csz;			/* Constant character size */
	int	e_lgm;			/* Ligature mode */
	int	e_lnn;			/* Current line number */
	int	e_lmn;			/* Line number multiple */
	int	e_lns;			/* Line number seperation */
	int	e_lni;			/* Line number indent */
	int	e_nnc;			/* Count for no number command */
	int	e_inpltrc;		/* Input line trap count */
	char	e_inptrap[2];		/* Input line trap */
	char	e_oldfon[2];		/* Last value of fon */
	int	e_oldpsz;		/* Last value of psz */
	int	e_oldvls;		/* Last value of vls */
	int	e_oldlln;		/* Last value of lln */
	int	e_oldind;		/* Last value of ind */
	int	e_oldmws;		/* Last value of mws */
	int	e_oldlsp;		/* Last value of lsp */
	int	e_oldtln;		/* Last value of tln */
	int	e_oldmar;		/* Last value of mar */
	char	e_ccc;			/* Current control character */
	char	e_nbc;			/* No break character */
	char	e_tbc;			/* Tab character */
	char	e_ldc;			/* Leader character */
	char	e_hic;			/* Hyphenation indicator character */
	char	e_tpc;			/* Page character in title */
	char	e_mrc;			/* Margin character */
	CODE	e_linebuf[LINSIZE];	/* Line buffer */
	CODE	e_wordbuf[WORSIZE];	/* Word buffer */
	char	e_newbold,		/* New value of bolmode */
		e_newital;		/* New value of itlmode */
	int	e_newfont,		/* New value of fontype */
		e_newpsz;		/* New value of psz */
} ENV;

/*
 * Global variables.
 */
extern	ENV	env;			/* Current enviroment */
extern	int	envinit[ENVSIZE];	/* If initialised */
extern	int	envstak[EVSSIZE];	/* Enviroment stack */
extern	int	envs;			/* Enviroment stack index */
extern	int	mapfont [8];			/* Font map for \fn */
