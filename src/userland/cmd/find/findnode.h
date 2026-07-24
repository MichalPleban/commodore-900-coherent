/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

typedef	struct	NODE	{
	struct	NODE	*n_left;
	struct	NODE	*n_right;
	union {
		unsigned n_val;
		char	*n_str;
		char	**n_strp;
		time_t	n_time;
	}	n_un;
	int	(*n_fun)();
	int	n_op;
	int	n_type;
}	NODE;
