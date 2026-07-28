#include "graph.h"

/******************************************************************************
	gkToGlobal(pt)	- Convert a logical point to global coordinates.
	gkToLogical(pt)	- Convert a global point to logical coordinates.
	gkToLogicalX(x) - Convert an x-coordinate to logical value.
	gkToLogicalY(y) - Convert a y-coordinate to logical value.
	gkToGlobalX(x) -  Convert an x-coordinate to a global value.
	gkToGlobalY(y)  - Convert a y-coordinate to a global value.

	Entry:	pt = point structure containing the logical absolute
		     or relative coordinate.

	Exit:	point structure or coordinate value
*******************************************************************************/

POINT gkToGlobal(pt)
POINT pt;
{
	pt.x -= gkLorigin.x - gkBitMap.rect.origin.x;
	pt.y -= gkLorigin.y - gkBitMap.rect.origin.y;
	return pt;
}


POINT gkToLogical(pt)
POINT pt;
{
	pt.x -= gkBitMap.rect.origin.x - gkLorigin.x ;
	pt.y -= gkBitMap.rect.origin.y - gkLorigin.y ;
	return pt;
}

int gkToGlobalX(x)
int x;
{
	return (x - gkLorigin.x + gkBitMap.rect.origin.x);
}

int gkToGlobalY(y)
int y;
{
	return (y - gkLorigin.y + gkBitMap.rect.origin.y);
}

int gkToLogicalX(x)
int x;
{
	return (x - gkBitMap.rect.origin.x + gkLorigin.x);
}

int gkToLogicalY(y)
int y;
{
	return (y - gkBitMap.rect.origin.y + gkLorigin.y);
}
