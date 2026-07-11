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
#include <drvcon.h>
#include <coherent.h>

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
