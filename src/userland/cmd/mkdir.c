/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * mkdir -- make directories
 */
#include	<dir.h>
#include	<sys/ino.h>
#include	<signal.h>
#include	<stdio.h>
#include	<errno.h>


#define	equal( s1, s2)	(strcmp( s1, s2) == 0)

int	interrupted;

char	*getparent(),
	*getchild(),
	*concat(),
	*malloc(),
	*strcat(),
	*strcpy(),
	*strncpy();

/*
 * main
 * Interrupts are handled to prevent the formation of mangled directories.
 */
main(argc, argv)
register char	**argv;
{
	register int status = 0;

	catch( SIGINT);
	catch( SIGHUP);
	signal( SIGQUIT, SIG_IGN);

	if (*++argv == NULL)
	{	fprintf(stderr, "Usage: mkdir dir ...\n");
		exit(1);
	}
	else
		while (*argv) {
			if (mkdir( *argv++) < 0)
			 	status = 1;  /* error */
			if (interrupted)
				exit(1);
		}
	exit(status);
}


/*
 * make a directory
 * If the parent exists and is writeable, the directory and its "." and
 * ".." links are created.
 */
mkdir( dir)
register char	*dir;
{
	register char	*parent,
			*child;

	if ((int) (child = getchild( dir)) < 0) {
		error("can't get child dir name %s", dir);
		return(-1);
	}
	parent = getparent( dir);
	if (equal( child, ".") || equal( child, "..")) {
		error("%s not allowed", dir);
		return(-1);
	}
	if (access( parent, 03))
		switch (errno) {
		case ENOENT:
			error("parent dir %s doesn't exist", parent);
			return(-1);
		case EACCES:
			error("no permission to mkdir in %s", parent);
			return(-1);
		default:
			noway( dir);
		}

	if (mknod( dir, IFDIR|0777, 0))
		switch (errno) {
		case EEXIST:
			error("%s already exists\n", dir);
			return(-1);
		case EPERM:
			error("not the super-user");
			return(-1);
		default:
			noway( dir);
		}
	if (link( dir, concat( dir, "/."))) {
		linkerr( dir, ".");
		return(-1);
	}
	if (link( parent, concat( dir, "/.."))) {
		linkerr( dir, "..");
		return(-1);
	}
	if (chown( dir, getuid( ), getgid( )) < 0)
	   return(-1);
}


/*
 * return name of parent
 */
char	*
getparent( dir)
char	*dir;
{
	register	i;
	register char	*p;
	static char	*par;
	int 		tmp;

	if (par)
		free( par);
	i = strlen( dir);
	par = malloc( i+1);
	if (par == NULL) {
		nomemory( );
		return(-1);
	}
	strcpy( par, dir);

	for (p=par+i; p>par; )
		if (*--p != '/')
			break;
	for (++p; *--p!='/'; )
		if (p == par) {
			*p = '.';
			break;
		}
	*++p = 0;
	if (par[tmp = strlen(par)-1] == '/')
	   par[tmp] = 0;  /* kill any ending slash */
	return (par);
}


/*
 * return rightmost component of pathname
 */
char	*
getchild( dir)
register char	*dir;
{
	register char	*p,
			*q;
	int		i;
	static char	ch[DIRSIZ+1];

	p = &dir[strlen( dir)];
	do {
		if (p == dir)
			fatal( -1, "don't be silly");
	} while (*--p == '/');
	q = p;
	while (q > dir)
		if (*--q == '/') {
			++q;
			break;
		}
	i = p+1 - q;
	if (i > DIRSIZ)
		i = DIRSIZ;
	return(strncpy( ch, q, i));
}


/*
 * return concatenation of `s1' and `s2'
 */
char	*
concat( s1, s2)
char	*s1,
	*s2;
{
	static char	*str;

	if (str)
		free( str);
	str = malloc( strlen( s1)+strlen( s2)+1);
	if (str == NULL) {
		nomemory();
		return(-1);
	}
	strcpy(str, s1);
	return(strcat( str, s2));
}


/*
 * recover from link failure
 * In the event that "." or ".." cannot be created, remove all traces
 * of the directory.
 */
linkerr( dir, name)
char	*dir,
	*name;
{
	unlink( concat( dir, "/."));
	unlink( concat( dir, "/.."));
	unlink( dir);
	error("link to '%s' failed", name);
	return(-1);
}


nomemory()
{
	error("out of memory");
	return(-1);
}


noway(dir)
char	*dir;
{
	error("can't make %s", dir);
	return(-1);
}


onintr( )
{

	signal( SIGINT, SIG_IGN);
	signal( SIGHUP, SIG_IGN);
	++interrupted;
}


catch( sig)
{

	if (signal( sig, SIG_IGN) == SIG_DFL)
		signal( sig, onintr);
}

error(arg1)
char	*arg1;
{
	fprintf( stderr, "mkdir: %r\n", &arg1);
}

fatal(arg1)
char 	*arg1;
{
	error(arg1);
	exit(1);
}
