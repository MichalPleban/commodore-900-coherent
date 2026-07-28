#include <stdio.h>
#include "smgr.h"


BITMAP	display;
LAYER	*DM_rearmost;
LAYER	*DM_frontmost;
WSTRUCT	*wtbl[MAX_WINDOWS]; 
WSTRUCT	gk;
MESSAGE	msg;
char	gkmsg[sizeof(GRAPH)+4];
POINT	SM_Mouse_Pos;
UPDATE	update[MAX_UPBUF];
