/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Modes for the access
 * system call.
 */

#define	AREAD	04		/* Test for read */
#define	AWRITE	02		/* Test for write */
#define	AEXEC	01		/* Test for execute */
#define	AAPPND	AWRITE		/* Test for append */

/* Dummy directory modes */
#define	ALIST	AREAD		/* List directory */
#define	ADEL	AWRITE		/* Delete directory entry */
#define	ASRCH	AEXEC		/* Search directory */
#define	ACREAT	AAPPND		/* Create directory entry */
