/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

typedef	struct	NODE	{
	struct	NODE	*n_left;
	struct	NODE	*n_right;
	union {
		int	(*n_fun)();
		int	n_op;
	}	n_un;
	char	*n_s1;
	char	*n_s2;
}	NODE;

