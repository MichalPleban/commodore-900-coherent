/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <drvcon.h>
#include <coherent.h>
#include <uproc.h>

int kvload();
int kvuload();
int kvopen();
int kvclose();
int kvioctl();
int v0read();
int v0write();
int nulldev();
int nonedev();

CON kvcon = {
	DFCHR,
	8,
	kvopen,
	kvclose,
	nonedev,
	v0read,
	v0write,
	kvioctl,
	nulldev,
	nulldev,
	kvload,
	kvuload
};
kvload()
{
	kbload();
	v0load();
}
kvuload()
{
	kbuload();
	v0uload();
}
kvopen(dev, mode)
dev_t dev; int mode;
{
	kbopen(dev, mode);
	v0open(dev, mode);
	if (u.u_error)		/* open failed - undo the keyboard open */
		kbclose(dev, mode);
}
kvclose(dev, mode)
dev_t dev; int mode;
{
	kbclose(dev, mode);
	v0close(dev, mode);
}
kvioctl(dev, com, vec)
dev_t dev; int com; char *vec;
{
	kbioctl(dev, com, vec);
	v0ioctl(dev, com, vec);
}
