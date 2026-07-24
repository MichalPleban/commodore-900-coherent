/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * execv(name, argv)
 * Sys exec with an array of arguments, but no
 * environment specified.
 */

extern char **environ;

execv(name, argv)
char *name;
char *argv[];
{
	execve(name, argv, environ);
}
