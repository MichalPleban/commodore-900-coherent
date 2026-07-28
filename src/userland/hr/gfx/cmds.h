
/* 
* Below is a general list of commands accepted by the screen manager.  This
* list is comprised principally of the window management functions.  Functions
* which relate to the graphics environment are documented elsewhere.  The 'C'
* defines are created in a hierarchical fashion to allow easy modification
* in the future.
*/

#define SM_BASE_CMD     (0)
#define SM_CREATE       (0) /* create new window            */
#define SM_CLOSE	(1) /* delete an existing window    */
#define SM_WRESET       (2) /* reset window parameters      */

#define SM_GETXCLIP     (3) /* get current clipping origin  */
#define SM_GETYCLIP     (4) /* get current clipping corner  */
#define SM_SETXCLIP     (5) /* set new clipping origin      */
#define SM_SETYCLIP     (6) /* set new clipping corner      */
#define SM_CLRCLIP      (7) /* clear out the clipping area  */

#define SM_GETPHY       (8) /* get physical window coords   */
#define SM_SETLOG       (9) /* set logical window origin    */
#define SM_GETLOG       (10) /* get logical window coords    */

#define SM_SETEVMSK     (11) /* set window event mask        */
#define SM_GETEVMSK     (12) /* get current event mask       */
#define SM_RSTEVMSK     (13) /* reset window event mask      */

#define SM_SETMPAT      (14) /* set mouse pattern for window */
#define SM_GETMPAT      (15) /* get mouse pattern for window */
#define SM_GETMOUSE     (16) /* get mouse logical position   */
#define SM_TRACK	(17)
#define SM_UNTRACK	(18)

#define SM_OPENFONT     (19) /* open font file               */
#define SM_CLOSEFONT    (20) /* decrement font use count     */
#define SM_GETFONTPARAM (21) /* return font header           */

#define SM_CHAR         (22) /* print a single character     */
#define SM_STRING       (23) /* print a character buffer     */
#define SM_CHARW        (24) /* return width of character    */
#define SM_STRW         (25) /* return width of string       */
#define SM_SCROLL       (26) /* scroll clipping region       */
#define SM_DEFCURS      (27) /* define cursor height & width */
#define SM_DRAWCURS     (28) /* draw (or erase? ) the cursor */

#define SM_SETGRAPH     (29) /* set the graphics state       */
#define SM_GETGRAPH     (30)  /* get the graphics state       */
#define SM_RSTGRAPH     (31)  /* reset the graphics state     */

#define SM_GETPOINT     (32)  /* get drawing point position   */
#define SM_RSTPOINT     (33)  /* reset drawing point position */
#define SM_POINT        (34) /* draw point absolute          */
#define SM_TOPOINT      (35) /* draw point relative          */
#define SM_MOVE         (36) /* move drawing point absolute  */
#define SM_TOMOVE       (37) /* move drawing point relative  */
#define SM_LINE         (38) /* draw line absolute           */
#define SM_TOLINE       (39) /* draw line relative           */
#define SM_POLY         (40) /* draw polygon absolute        */
#define SM_TOPOLY       (41) /* draw polygon relative        */
#define SM_RECT         (42) /* draw rectangle               */
#define SM_RRECT        (43) /* draw rounded rectangle       */
#define SM_OVAL         (44) /* draw oval                    */
#define SM_WEDGE        (45) /* draw wedge                   */
#define SM_ARC          (46) /* draw arc                     */

#define SM_UPDATE	(47)
#define SM_UPDALL	(48)
#define SM_FRONT	(49)
#define SM_BACK		(50)
#define SM_WHOTOPAT	(51)
#define SM_WHOFRONT	(52)
#define SM_SETPHY	(53)
#define SM_SETSIZE	(54)

#define SM_AMAP		(55)

#define SM_TOP_CMD	(SM_AMAP)


#define WM_BASE_CMD     (0)
#define WM_REPLY        (0) /* a return message         */
#define WM_ACK          (1) /* general ACK              */
#define WM_NACK         (2) /* general NACK             */
#define WM_UPDATE       (7) /* please update your area  */
#define WM_RGNUPDATE	(8)
#define WM_URGONE       (9) /* you've been deleted      */
#define WM_SIZECHANGE   (10) /* your dimenstions changed */
#define WM_CHARACTER    (11) /* input character          */
#define WM_MMOVE        (12) /* the mouse moved          */
#define WM_MTRANS       (13) /* mouse button transition  */
#define WM_NEWFONT	(14)
#define WM_TOP_CMD      (WM_NEWFONT) /* highest legal command    */

