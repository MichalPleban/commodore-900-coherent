/*
 *		externs 
 */


extern	void	GK_SetGraph();
extern	void	GK_GetGraph();
extern	void	GK_RstGraph();
extern	void	GK_GetPoint();
extern	void	GK_RstPoint();
extern	void	GK_Point();
extern	void	GK_ToPoint();
extern	void	GK_Move();
extern	void	GK_ToMove();
extern	void	GK_Line();
extern	void	GK_ToLine();
extern	void	GK_Poly();
extern	void	GK_ToPoly();
extern	void	GK_Rect();
extern	void	GK_Rrect();
extern	void	GK_Oval();
extern	void	GK_Wedge();
extern	void	GK_Arc();

extern	RECT	OffsetRect();
extern	RECT	InsetRect();
extern	RANGE	*rgRange();
extern	RANGE	*rgRast();
extern	POINT	gkToGlobal();
extern	POINT	gkToLogical();
extern	int	gkfill();
extern	WSTRUCT	gk;
extern	MESSAGE	msg;
extern	char	gkmsg[];
extern	int	*texture[];
extern  int 	*screen_addr();	
extern  int	words_between();
extern  LAYER 	*DM_frontmost,		/* frontmost layer pointer */
		 *DM_rearmost;		/* rearmost layer pointer  */
extern  WSTRUCT	*wtbl[];		/* table of window pointers */
extern  int 	SM_LastWidSeen;		/* last window touched, or -1 */
