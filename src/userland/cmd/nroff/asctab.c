/*
 * Nroff/Troff.
 * Character table.
 */
#include <stdio.h>
#include "roff.h"
#include "char.h"

/*
 * Map from ASCII to internal format.
 */
int asctab[] ={
	CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,
	CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,
	CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,
	CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,   CNULL,
	CNULL,   CEXCLAM, CDQUOTE, CNUMBER, CDOLLAR, CPERCEN, CAMPER,  CCQUOTE,
	CLPAREN, CRPAREN, CASTER,  CPLUS,   CCOMMA,  CHYPHEN, CDOT,    CSLASH,
	C0,      C1,      C2,      C3,      C4,      C5,      C6,      C7,
	C8,      C9,      CCOLON,  CSEMI,   CLTHAN,  CEQUAL,  CGTHAN,  CQUEST,
	CATSIGN, CUA,     CUB,     CUC,     CUD,     CUE,     CUF,     CUG,
	CUH,     CUI,     CUJ,     CUK,     CUL,     CUM,     CUN,     CUO,
	CUP,     CUQ,     CUR,     CUS,     CUT,     CUU,     CUV,     CUW,
	CUX,     CUY,     CUZ,     CLSQBR,  CBACKSL, CRSQBR,  CUARROW, CUNDERS,
	COQUOTE, CLA,     CLB,     CLC,     CLD,     CLE,     CLF,     CLG,
	CLH,     CLI,     CLJ,     CLK,     CLL,     CLM,     CLN,     CLO,
	CLP,     CLQ,     CLR,     CLS,     CLT,     CLU,     CLV,     CLW,
	CLX,     CLY,     CLZ,     CLBRACE, CORBAR,  CRBRACE, CTILDE,  CNULL
};
