/*
 * Configuration table for the Keyboard/video
 * driver.  The keyboard and the video part
 * are separate modules and have only a well-defined
 * interface to each other.
 */

#include <coherent.h>
#include <drvcon.h>

/*
 * Common entry points
 */
int	nulldev();
int	nonedev();
int	kvopen();
int	kvclose();
int	kvioctl();
int	kvload();
int	kvuload();
/*
 * Keyboard entry points
 */
int	kbload();
int	kbuload();
int	kbopen();
int	kbclose();
/*
 * Memory mapped video entry points
 */
int	v0load();
int	v0uload();
int	v0read();
int	v0write();
int	v0open();
int	v0close();

CON	kvcon	=	{		/* Keyboard/Video */
	DFCHR,				/* Flags */
	8,				/* Major index */
	kvopen,				/* Open */
	kvclose,			/* Close */
	nonedev,			/* Block */
	v0read,				/* Read */
	v0write,			/* Write */
	kvioctl,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	kvload,				/* Load */
	kvuload,			/* Unload */
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

kvopen(dev, m)
dev_t dev;
int m;
{
	kbopen(dev, m);
	v0open(dev, m);
}

kvclose(dev)
dev_t dev;
{
	kbclose(dev);
	v0close(dev);
}

kvioctl(dev, com, p)
dev_t dev;
int com;
char *p;
{
	kbioctl(dev, com, p);
	v0ioctl(dev, com, p);
}
