/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Execl(name, arg0, arg1, ..., argn, NULL)
 * Sys exec with a list of arguments and no environment
 * given.
 */

extern	char	**environ;

execl(name, arg0)
char *name;
char *arg0;
{
	execve(name, &arg0, environ);
}
