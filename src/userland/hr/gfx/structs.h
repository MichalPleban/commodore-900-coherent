
 /*************************************************************************
 *
 *
 *		STRUCTURES
 *
 *
 *************************************************************************/
typedef int	PAT[16];


typedef struct Point
{
	int x;
	int y;
} POINT;

typedef struct 
{
	POINT origin;  		/* top left corner of rectangle */
	POINT corner;		/* bottom right corner of rectangle */
} RECT;


typedef struct Bitmap
{
	int  *base;              /* top left word address */
	unsigned width;         /* width of rectangle in pixels */
	RECT rect;              /* image rectangle */
} BITMAP;

typedef struct Sm_Region
{
	BITMAP	bm;
	int	flag;
	struct Layer *cov_by;
} SM_REGION;

/* Visible-region list capacity per layer.  Each SM_REGION is ~20 bytes and a
 * LAYER embeds MAX_LRBUF of them, so this dominates sizeof(LAYER) and the heap
 * cost of every window.  wserver's data segment is near the Z8001 64K limit
 * (~2.8K heap), so 100 (which made a LAYER 2022 bytes -> only one window fit)
 * is far too many: a handful of overlapping windows split a layer into only a
 * few rectangles.  32 keeps a LAYER ~662 bytes so several windows coexist. */
#define MAX_LRBUF	(32)

typedef struct Layer 
{
	int  *base;              /* start of data */
	unsigned width;         /* width of rectangle in pixels */
	RECT rect;              /* image rectangle */
	SM_REGION reg[MAX_LRBUF];	/* list of visible regions */	
	struct Layer *front;    /* adjacent layer in front */
	struct Layer *back;     /* adjacent layer in back */
} LAYER;


typedef struct {
	BITMAP 	*src;
	POINT 	sp;
	LAYER 	*dst;
	RECT 	dr;
	int	op;
	int	*pat;
	} BLTSTRUCT;



typedef struct
{
	char	pn_Width;	/* Pen width			*/
	char	pn_Height;	/* Pen height			*/
	char	pn_Pat;		/* Pen fill pattern		*/
} PEN;


typedef struct FontInfo
{
	char	fi_Id;		/* font identifier, or index	*/
	char	fi_Style;	/* font style (underlined, etc)	*/
	POINT	fi_Scalex;	/* font scale factor in x-dir	*/
	POINT	fi_Scaley;	/* font scale factor in y-dir	*/
} FONTINFO;



/*
**  Graphics Drawing Characteristics
*/
typedef struct
{
	PEN	 wn_Pen;	/* working pen				    */
	char	 wn_Logop;	/* bit-blt logical operation		    */
	char	 wn_Fpat;	/* SOLID, HORIZONTAL, VERTICAL, CROSSHATCH  */
	char  	 wn_Bpat;	/* background fill pattern		    */
	char	 wn_Fcolor;	/* foreground color (background is opposite)*/
	char	 wn_Bcolor;	/* background color			    */
	FONTINFO wn_Font;	/* font data				    */
        PAT	 wn_Pattern;	/* user definable fill pattern		    */
} GRAPH;



typedef struct
{
	int	xl;		/* low end of range	*/
	int	xh;		/* high end of range	*/
} RANGE;


typedef struct
{
	int	rg_Size;	/* size, in #ints, of variable length area  */
	RECT	rg_Rect;	/* enclosing rectangle                      */
	RANGE	rg_Rast[1];	/* raster table plus range data, var length */
} REGION;


typedef struct
{
	int	py_Size;	/* number of vertices in the polygon	*/
	POINT	py_Point[1];	/* variable length array of vertices	*/
} POLYGON;





typedef struct Wstruct
{
	int	wn_Type;	/* window type                             */
	int	wn_Wid;		/* Window id.. entry in wtbl               */
	int	wn_Wmgr;	/* Owning window manager                   */
	int 	wn_Flags;	/* bit 0: 1=fully visible                  */
	LAYER	*wn_Layer;	/* display structure                       */
	POINT	wn_Psize;	/* width & height in pixels                */
	POINT	wn_Dp;		/* drawing point position		   */
	GRAPH	wn_G;		/* graphics characteristics		   */
	POINT	wn_Lorigin;	/* logical coordinate system		   */
	RECT    wn_Crect;	/* content rectangle, physical coordinates */
	int 	wn_Evmask;	/* window event mask                       */
	POINT 	wn_Cursor;	/* cursor height & width                   */
	int	wn_Cascnt;	/* cursor ascent			   */
	POINT 	wn_Curpos;	/* current cursor position      PTB        */
				/* This is the last known drawing point    */
	int	*wn_ascii;	/* pointer to window managers ascii map    */
} WSTRUCT;


typedef struct Win_create
{
	char	wc_type;	/* window type			*/
	RECT	wc_dims;	/* outer boundarys              */
	int	*wc_ascii;	/* pointer to wmgr ascii map	*/
} WIN_CREATE;



/*
 *	Indirect message data structures
 */
typedef struct
{
	char	sm_p;		/* pattern if paint mode	*/
	char	sm_verb;	/* drawing mode			*/
	int	sm_nv;		/* number of vertices		*/
	POINT	*sm_vs;		/* pointer to vertices		*/
} MPOLY;

typedef struct
{
	char	sm_p;		/* pattern if paint mode	*/
	char	sm_verb;	/* drawing mode			*/
	RECT	sm_rect;	/* enclosing rectangle		*/
	int	sm_intval1;	/* angle or corner width	*/
	int	sm_intval2;	/* angle or corner height	*/
	char	sm_chrval1;	/* an internal byte field	*/
	char	sm_chrval2;	/* another internal byte field	*/
} MSHAPE;



/* strictly for internal screen manager use. */
typedef struct Update {
	RECT r;
	int  wid;
	int  wmgr;
} UPDATE;
