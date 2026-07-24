/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent Library Internals
 * This routine is placed in library after exit
 * to be loaded if there are no references to standard I/O,
 * displacing the routine of the same name in standard I/O
 * who drags in the world.
 */
_finish(){}
