/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * reboot - flush the file systems and restart the machine.
 *
 * The reboot() system call marks all file systems clean, flushes the
 * buffer cache and jumps to the ROM restart entry; it only returns on
 * failure (for example when not run by the super-user).
 */

main()
{
	sync();
	sync();
	reboot();
	write(2, "reboot: cannot reboot (not super-user?)\n", 40);
	return (1);
}
