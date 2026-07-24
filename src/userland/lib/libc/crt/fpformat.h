/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* floating point package for segmented z-8001
 timothy s. murphy  10/84
 IEEE format
 double:	63 62		52 51				0
	      sign  bin exp +1022   fraction (missing hi bit)
 float:	31 30		23 22				0
	      sign  bin exp +126    fraction (missing hi bit)
*/

#define	EBITS	11
#define	MBITS	52
#define	FEBITS	8
#define	FMBITS	23
