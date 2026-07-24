/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>


int chunk = 1000;		/* number of lines per output file */
FILE *infile = stdin;		/* input file */
char *outname = "xaa";		/* first output file */
char *tail;			/* points to last character of outname */




/*
 *	Split takes a file copies it into n-line pieces on files with
 *	names that end in aa thru zz.  Usage is as follows:
 *		split [-n] [infile [outfile]]
 *	where "n" is the number of lines to put per file (default = 1000),
 *	"infile" is the file to read (default or "-" is standard input)
 *	and "outfile" is the initial part of the file name (default is "x").
 */

main(argc, argv)
int argc;
char *argv[];
{
	int ch;
	FILE *outfile;

	options(argc, argv);
	while (!feof(infile) && (ch = getc(infile)) != EOF) {
		ungetc(ch, infile);
		outfile = fopen(outname, "w");
		if (outfile == NULL) {
			fprintf(stderr, "split: can't open %s\n", outname);
			exit(1);
		}
		copylines(infile, outfile, chunk);
		fclose(outfile);
		nextname(outname);
	}
}


/*
 *	Options processes argc/argv options.
 */

options(argc, argv)
register int argc;
register char *argv[];
{
	FILE *getinput();
	char *getoutname();

	--argc;
	++argv;

	if (argc > 0 && **argv == '-') {
		chunk = atoi(++*argv);
		if (chunk <= 0)
			usage();
		++argv;
		--argc;
	}

	if (argc > 2)
		usage();

	if (argc > 0) {
		infile = getinput(*argv++);
		--argc;
	}

	if (argc > 0) {
		outname = getoutname(*argv++);
		--argc;
	}
	tail = outname + strlen(outname) - 1;
}


/*
 *	Usage simply issues the usage message and calls exit.
 */

usage()
{
	fprintf(stderr, "usage: split [-n] [infile [outfile]]\n");
	exit(1);
}


/*
 *	Getinput returns the input stream corresponing to the string
 *	"str".  If this is "-" then stdin is returned.  If the file
 *	cannot be opened then getinput calls exit after issueing an
 *	appropriate error message.
 */

FILE *
getinput(str)
char *str;
{
	register FILE *result;

	if (strcmp(str, "-") == 0)
		return (stdin);
	result = fopen(str, "r");
	if (result != NULL)
		return (result);
	fprintf(stderr, "split: can't open %s\n", str);
	exit(1);
}


/*
 *	Getoutname returns a pointer to a string which has a copy of
 *	the file name in "str" with aa added to the end.
 */

char *
getoutname(str)
char *str;
{
	register char *result;
	int length;
	char	*malloc();

	length = strlen(str) + 2;
	result = (char *) malloc(length + 1);
	if (result == NULL) {
		fprintf(stderr, "split: %s too long\n", str);
		exit(1);
	}
	strcpy(result, str);
	result[length - 1] = result[length - 2] = 'a';
	return (result);
}


/*
 *	Copylines is used to copy "num" lines from the stream "in" to
 *	the stream "out".
 */

copylines(in, out, num)
register FILE *in, *out;
int num;
{
	register int ch;

	while ((ch = getc(in)) != EOF) {
		putc(ch, out);
		if (ch == '\n' && --num <= 0)
			return;
	}
}


/*
 *	Nextname sets outfile to the next name to use for an output file.
 *	If there are no more output files, then it calls exit after issueing
 *	an appropriate error message.
 */

nextname()
{
	if (++*tail <= 'z')
		return;
	*tail = 'a';
	if (++*(tail - 1) <= 'z')
		return;
	fprintf(stderr, "split: too many output files\n");
	exit(1);
}
