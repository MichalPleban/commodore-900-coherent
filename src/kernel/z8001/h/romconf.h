/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
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
