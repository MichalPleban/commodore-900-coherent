/*
 * Nroff/Troff.
 * Tables.
 */
#include <stdio.h>
#include "roff.h"
#include "esc.h"
#include "font.h"

/*
 * Define all request functions.
 */
int	req_ab();
int	req_ad();
int	req_af();
int	req_am();
int	req_as();
int	req_bp();
int	req_br();
int	req_c2();
int	req_cc();
int	req_ce();
int	req_ch();
int	req_cs();
int	req_cu();
int	req_da();
int	req_de();
int	req_di();
int	req_ds();
int	req_dt();
int	req_ec();
int	req_el();
int	req_em();
int	req_eo();
int	req_ev();
int	req_ex();
int	req_fi();
int	req_fl();
int	req_ft();
int	req_fp();
int	req_ie();
int	req_if();
int	req_ig();
int	req_in();
int	req_it();
int	req_lg();
int	req_ll();
int	req_ls();
int	req_lt();
int	req_mc();
int	req_mk();
int	req_na();
int	req_ne();
int	req_nf();
int	req_nh();
int	req_nm();
int	req_nn();
int	req_nr();
int	req_ns();
int	req_nx();
int	req_os();
int	req_pc();
int	req_pl();
int	req_pm();
int	req_pn();
int	req_po();
int	req_ps();
int	req_rd();
int	req_rm();
int	req_rn();
int	req_rr();
int	req_rs();
int	req_rt();
int	req_so();
int	req_sp();
int	req_sv();
int	req_ta();
int	req_tc();
int	req_ti();
int	req_tl();
int	req_tm();
int	req_tr();
int	req_uf();
int	req_ul();
int	req_vs();
int	req_wh();
int	req_zz();

/*
 * Table giving what a character which is escaped maps onto.
 */
char esctab[ASCSIZE] ={
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, EIGN, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	EUNP, ETLI, ECOM, ENUL, EARG, EHYP, ENOP, EACA,
	ECHR, ENUL, ESTR, ENUL, ENUL, EMIN, ENUL, ENUL,
	EDWS, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, EVLF, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, EM12, ENUL,
	ENUL,  SOH, EBRA, EINT, EVNF, EESC, EFON, ENUL,
	EHMT, ENUL, ENUL, EMAR, EHLF, ENUL, ENUM, EOVS,
	ESPR, ENUL, EVRM, EPSZ, '\t', EVRN, EVMT, EWID,
	EXLS, ENUL, EZWD, EBEG, EM06, EEND, ENUL, ENUL
};

/*
 * Translate table.
 */
char trantab[ASCSIZE] ={
	0000, 0001, 0002, 0003, 0004, 0005, 0006, 0007,
	0010, 0011, 0012, 0013, 0014, 0015, 0016, 0017,
	0020, 0021, 0022, 0023, 0024, 0025, 0026, 0027,
	0030, 0031, 0032, 0033, 0034, 0035, 0036, 0037,
	0040, 0041, 0042, 0043, 0044, 0045, 0046, 0047,
	0050, 0051, 0052, 0053, 0054, 0055, 0056, 0057,
	0060, 0061, 0062, 0063, 0064, 0065, 0066, 0067,
	0070, 0071, 0072, 0073, 0074, 0075, 0076, 0077,
	0100, 0101, 0102, 0103, 0104, 0105, 0106, 0107,
	0110, 0111, 0112, 0113, 0114, 0115, 0116, 0117,
	0120, 0121, 0122, 0123, 0124, 0125, 0126, 0127,
	0130, 0131, 0132, 0133, 0134, 0135, 0136, 0137,
	0140, 0141, 0142, 0143, 0144, 0145, 0146, 0147,
	0150, 0151, 0152, 0153, 0154, 0155, 0156, 0157,
	0160, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
	0170, 0171, 0172, 0173, 0174, 0175, 0176, 0177
};

/*
 * For forming registers containing requests.
 */
REQ reqtab[] ={
	'a', 'b', req_ab,
	'a', 'd', req_ad,
	'a', 'f', req_af,
	'a', 'm', req_am,
	'a', 's', req_as,
	'b', 'p', req_bp,
	'b', 'r', req_br,
	'c', '2', req_c2,
	'c', 'c', req_cc,
	'c', 'e', req_ce,
	'c', 'h', req_ch,
	'c', 's', req_cs,
	'c', 'u', req_cu,
	'd', 'a', req_da,
	'd', 'e', req_de,
	'd', 'i', req_di,
	'd', 's', req_ds,
	'd', 't', req_dt,
	'e', 'c', req_ec,
	'e', 'l', req_el,
	'e', 'm', req_em,
	'e', 'o', req_eo,
	'e', 'v', req_ev,
	'e', 'x', req_ex,
	'f', 'i', req_fi,
	'f', 'l', req_fl,
	'f', 'p', req_fp,
	'f', 't', req_ft,
	'i', 'e', req_ie,
	'i', 'f', req_if,
	'i', 'g', req_ig,
	'i', 'n', req_in,
	'i', 't', req_it,
	'l', 'g', req_lg,
	'l', 'l', req_ll,
	'l', 's', req_ls,
	'l', 't', req_lt,
	'm', 'c', req_mc,
	'm', 'k', req_mk,
	'n', 'a', req_na,
	'n', 'e', req_ne,
	'n', 'f', req_nf,
	'n', 'h', req_nh,
	'n', 'm', req_nm,
	'n', 'n', req_nn,
	'n', 'r', req_nr,
	'n', 's', req_ns,
	'n', 'x', req_nx,
	'o', 's', req_os,
	'p', 'c', req_pc,
	'p', 'l', req_pl,
	'p', 'm', req_pm,
	'p', 'n', req_pn,
	'p', 'o', req_po,
	'p', 's', req_ps,
	'r', 'd', req_rd,
	'r', 'm', req_rm,
	'r', 'n', req_rn,
	'r', 'r', req_rr,
	'r', 's', req_rs,
	'r', 't', req_rt,
	's', 'o', req_so,
	's', 'p', req_sp,
	's', 'v', req_sv,
	't', 'a', req_ta,
	't', 'c', req_tc,
	't', 'i', req_ti,
	't', 'l', req_tl,
	't', 'm', req_tm,
	't', 'r', req_tr,
	'u', 'f', req_uf,
	'u', 'l', req_ul,
	'v', 's', req_vs,
	'w', 'h', req_wh,
	'z', 'z', req_zz,
	'\0'
};

/*
 * Table for putting out roman numerals.
 */
ROM romtab[10] ={
	0, 0,
	0, 0,
	0, 1,
	0, 2,
	1, 1,
	1, 0,
	0, 5,
	0, 6,
	0, 7,
	2, 1
};
