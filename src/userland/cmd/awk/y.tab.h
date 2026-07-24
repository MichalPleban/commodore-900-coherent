/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#line 24 "awk.y"
typedef union {
	int	u_char;
	char	*u_charp;
	NODE	*u_node;
	int	(*u_func)();
} YYSTYPE;
#define IF_ 256
#define WHILE_ 257
#define FOR_ 258
#define ELSE_ 259
#define BREAK_ 260
#define CONTINUE_ 261
#define NEXT_ 262
#define EXIT_ 263
#define IN_ 264
#define PRINT_ 265
#define PRINTF_ 266
#define BEGIN_ 267
#define END_ 268
#define FAPPEND_ 269
#define FOUT_ 270
#define ID_ 271
#define STRING_ 272
#define NUMBER_ 273
#define FUNCTION_ 274
#define SCON_ 294
#define ASADD_ 295
#define ASSUB_ 296
#define ASMUL_ 297
#define ASDIV_ 298
#define ASMOD_ 299
#define OROR_ 300
#define ANDAND_ 301
#define NMATCH_ 302
#define EQ_ 303
#define NE_ 304
#define GE_ 305
#define LE_ 306
#define INC_ 307
#define DEC_ 308
#define REEOL_ 309
#define REBOL_ 310
#define REANY_ 311
#define RECLASS_ 312
#define RECHAR_ 313
#define REOR_ 314
#define RECON_ 315
#define RECLOS_ 316
#define RENECL_ 317
#define REZOCL_ 318
#ifdef YYTNAMES
extern struct yytname
{
	char	*tn_name;
	int	tn_val;
} yytnames[];
#endif
extern	YYSTYPE	yylval;
