/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#line 16 "sh.y"
typedef union {
	NODE	*yu_node;
	char	*yu_strp;
	int	yu_nval;
} YYSTYPE;
#define _NULL 256
#define _DSEMI 257
#define _ANDF 258
#define _ORF 259
#define _NAME 260
#define _IORS 261
#define _ASGN 262
#define _CASE 263
#define _DO 264
#define _DONE 265
#define _ELIF 266
#define _ELSE 267
#define _ESAC 268
#define _FI 269
#define _FOR 270
#define _IF 271
#define _IN 272
#define _THEN 273
#define _UNTIL 274
#define _WHILE 275
#define _OBRAC 276
#define _CBRAC 277
#ifdef YYTNAMES
extern struct yytname
{
	char	*tn_name;
	int	tn_val;
} yytnames[];
#endif
extern	YYSTYPE	yylval;
