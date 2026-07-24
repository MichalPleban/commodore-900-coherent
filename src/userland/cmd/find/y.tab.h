/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#line 27 "find.y"
typedef union {
	NODE  *nodeptr;
	} YYSTYPE;
#define OR 256
#define AND 257
#define NAME 258
#define PERM 259
#define TYPE 260
#define LINKS 261
#define USER 262
#define GROUP 263
#define SIZE 264
#define INUM 265
#define ATIME 266
#define CTIME 267
#define MTIME 268
#define EXEC 269
#define OK 270
#define PRINT 271
#define NEWER 272
#define FUN 273
#define NOP 274
#ifdef YYTNAMES
extern struct yytname
{
	char	*tn_name;
	int	tn_val;
} yytnames[];
#endif
extern	YYSTYPE	yylval;
