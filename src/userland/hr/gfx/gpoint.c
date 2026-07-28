#include "graph.h"

/******************************************************************************
	SM_GetPoint()	- Return the drawing point position.
	SM_RstPoint()	- Reset the drawing point position.
	SM_Point()	- Move the drawing point absolutely and draw pen.
	SM_ToPoint()	- Move the drawing point relatively and draw pen.
	SM_Move()	- Move the drawing point absolutely.
	SM_ToMove()	- Move the drawing point relatively.
	SM_Line()	- Draw line from current point to absolute point.
	SM_ToLine()	- Draw line from current point to relative point.

	Entry:	msgPt = point structure containing the logical absolute
		        or relative coordinate.

	Exit:	Nothing is returned, except for SM_GetPoint(). The drawing
		point position is affected by most of these operations.
		The drawing point may be moved outside the visible part of
		logical coordinate plane. However, the drawing point cannot
		be moved so as to cause the drawing point pen to wraparound
		integerwise.
*******************************************************************************/
void SM_RstPoint()
{
	gkDp = gkCrect.origin;
}


void SM_GetPoint()
{
	msgCmd = WM_REPLY;
	msgPt = gkToLogical(gkDp);
	sendmsg(&msg);
}


void SM_Move()
{
	POINT	pt;

	pt = gkToGlobal(msgPt);
	gkDp.x = MIN(pt.x, 0x7fff-gkPen.pn_Width);
	gkDp.y = MIN(pt.y, 0x7fff-gkPen.pn_Height);
#ifdef DEBUG
	peteprint("move\n");
#endif
}


void SM_ToMove()
{
	msgPt.x += gkToLogicalX(gkDp.x);
	msgPt.y += gkToLogicalY(gkDp.y);
	SM_Move();
}


void SM_Point()
{
	BLTSTRUCT blt;

	SM_Move();
	blt.src = &gkBitMap;
	blt.dst = &gkBitMap;
	blt.dr.origin = gkDp;
	blt.dr.corner.x = blt.dr.origin.x + gkPen.pn_Width;
	blt.dr.corner.y = blt.dr.origin.y + gkPen.pn_Height;
	blt.dr = R_Intersection(gkCrect, blt.dr);
	blt.sp = blt.dr.origin;
	blt.op = gkLogop;
	blt.pat = gkTexture(gkPen.pn_Pat);
	lbitblt(&blt, 1, 1);
}


void SM_ToPoint()
{
	msgPt.x += gkToLogicalX(gkDp.x);
	msgPt.y += gkToLogicalY(gkDp.y);
	SM_Point();
}


void SM_Line()
{
	POINT	pt0;

	pt0 = gkDp;
	SM_Move();
	gkLine(pt0, gkDp, gkPen.pn_Pat, gkLogop, gkPen.pn_Width);
}


void SM_ToLine()
{
	msgPt.x += gkToLogicalX(gkDp.x);
	msgPt.y += gkToLogicalY(gkDp.y);
	SM_Line();
}
