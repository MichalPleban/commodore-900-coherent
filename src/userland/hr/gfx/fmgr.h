# define FM_MAXFONT        8      /* maximum number of fonts      */
# define FM_MAXNLEN       12      /* font maximum name length     */
# define FM_MAXCHAR       256     /* maximum number of characters */
# define FNOT_EXST        -1      /* font does not exist          */
# define MEM_ALLOC_FAIL   -2      /* memory allocation failed     */
# define FILE_OPEN_FAIL   -3      /* file open failed             */
# define FONT_NULL         0     
# define TAB_FILLED       -2      /* active font table filled 
			             8 fonts are already loaded   */
# define FONT_NOT_OPEN    -1      /* font file not open           */
# define FCLOSE_OK         0      /* font file close successful   */

# define SYS_FID           0      /* system font id               */
# define SYS_FNAME   "sysfont"    /* system font file name        */
# define SYS_FNULL   "\0"         /* system font file name null   */


       /* library structure */
typedef struct Font_Header
{
   int    ffirst_char;      /* ASCII of first defined character      */
   int    flast_char;       /* ASCII of last defined character       */
   char   font_ascent;      /* # of raster lines above base line     */
   char   font_descent;     /* # of raster lines below base line     */
   char   font_leading;     /* # of blank lines between rows of text */
   char   fmax_width;       /* maximum character width               */
   char   fmin_width;       /* minimum character width               */
   char   ffont_name[FM_MAXNLEN];   /* name of font file             */
   char   unused1;          /* may be maxkern field                  */
   char   unused2;          /* may be maxfwidth field                */
   char   unused3;          /* only because pete insisted            */
   char   fopen_count;      /* # of applications using a font        */
   BITMAP font_map;         /* the font bitmap structure             */
   int    *loctable;        /* pointer to location table  
			       bit offset to font rectangle
			       sizeof(flast_char - ffirst_char + 3)  */
   int    *kwtable;         /* pointer to kern/width table
			       high byte = amount of kern
			       low byte  = width of character rect.
			       sizeof(flast_char - ffirst_char + 3)  */
} FONT_HEADER;


       /* IOCTL structures */

typedef struct Getptr
{
   int    gfid;            /* font id number                         */
   int    gch;             /* ascii character                        */
   BITMAP gbmap;           /* bitmap structure filled by FM_getch()  */
   POINT  gpoint;          /* top left corner of character           */
   int    gkern,           /* byte which  represents the amount by 
			      which to back up the cursor prior to 
			      printing the character                 */
          gascent,         /* # of raster lines to move up before
			      starting to print the top raster of
			      the character                          */
	  gdescent,        /* # of raster lines below the base line  */
          gleading,        /* # of raster lines the cursor should
			      drop down                              */
          gawdth;          /* actual width of stored character       */
} GETPTR;
