#ifdef DEBUG
#include <stdio.h>
#endif
#include <smgr.h>

extern	void	SM_Char();
extern	void	SM_Str();
extern	void	SM_CharW();
extern	void	SM_StrW();
extern	void	SM_DefCurs();
extern	void	SM_DrawCurs();
extern	void	SM_SetGraph();
extern	void	SM_GetGraph();
extern	void	SM_RstGraph();
extern	void	SM_GetPoint();
extern	void	SM_RstPoint();
extern	void	SM_Point();
extern	void	SM_ToPoint();
extern	void	SM_Move();
extern	void	SM_ToMove();
extern	void	SM_Line();
extern	void	SM_ToLine();
extern	void	SM_Poly();
extern	void	SM_ToPoly();
extern	void	SM_Rect();
extern	void	SM_Rrect();
extern	void	SM_Oval();
extern	void	SM_Wedge();
extern	void	SM_Arc();

extern	RECT	OffsetRect();
extern	RECT	InsetRect();

extern	POINT	gkToGlobal();
extern	POINT	gkToLogical();
extern	char	*malloc();
extern	char	*calloc();
extern	int	gkfill();
extern	char	*LockPtr();
extern	MESSAGE	msg;
extern	char	gkmsg[];
extern	int	*texture[];

extern	RECT	R_Intersection(),
		R_inset(),
		R_addp();

extern	uint	*addptr(),
		*subptr();
