/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Execlp(name, arg0, arg1, ..., argn, NULL)
 * Repeatedly do sys exec on each of the directories in the
 * search rules found in the environment.
 */

#include <stdio.h>

/* VARARGS */
execlp(name, arg0)
char *name;
char *arg0;
{
	return (execvp(name, &arg0));
}
