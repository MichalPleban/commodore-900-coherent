/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Configuration information about the machine is generated
 * by the ROM.  This structure (located at location ROMLOC
 * in segment 0, contains the information.
 */
struct	romconf	{
	unsigned rom_bram;		/* Beginning of ram 512 byte click */
	unsigned rom_eram;		/* End of ram in 512 byte clicks */
	char	*rom_auto;		/* Autoboot command (NULL is Q/A) */
	int	*rom_restart;		/* Restart address */
	char	rom_fdtype;		/* Type of floppy disc */
	char	rom_hdtype;		/* Type of hard disc (0 = none) */
	char	rom_ctype;		/* Clock types 0=4Mhz, 1=6Mhz */
};
