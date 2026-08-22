/*
 * vellum.h - shared types and externs of the Vellum drawing tool.
 *
 * Vellum is several translation units now (vellum.c the editor proper,
 * velfile.c the plain-text file format, velport.c the headless print /
 * pic / netlist exporters): one .c grew past what the Z8001 assembler's
 * fix-up tables can track in a single module.  Everything the units
 * share -- the object model, the symbol-library pools, the layer state
 * -- is declared here and defined once in vellum.c.
 */

/* ---- objects ---- */
#define	OT_SYM	1
#define	OT_WIRE	2
#define	OT_LINE	3
#define	OT_BOX	4
#define	OT_CIRC	5
#define	OT_TEXT	6
#define	OT_SHAPE 7		/* S: stretchable labeled outline         */
#define	OT_CONN	8		/* K: connector, endpoints may ATTACH     */
#define	OT_POLY	9		/* P: polyline (points in ppool)          */
#define	OT_ARC	10		/* A: circular arc                        */
#define	OT_NNAME 11		/* N: explicit net name at a point        */
#define	OT_DIM	12		/* D: dimension (ticks, arrows, label)    */

/* OT_SHAPE kinds (o_sym), drawn parametrically from the bounding rect */
#define	SH_BOX	0
#define	SH_RBOX	1
#define	SH_DIAM	2
#define	SH_OVAL	3
#define	SH_PAR	4
#define	SH_DRUM	5
#define	SH_DOC	6
#define	SH_CIRC	7
#define	SH_ELL	8		/* true ellipse (the oval is a stadium)   */
#define	NSHAPE	9

/* OT_CONN styles (o_sym) */
#define	CS_HV	0		/* H-then-V, the wire route               */
#define	CS_LINE	1		/* straight                               */
#define	CS_ARROW 2		/* straight, arrowhead at b               */
#define	CS_HARROW 3		/* H-V, arrowhead at b                    */
#define	NCONNS	4

/* o_flags: line style (2 bits) + fill (2 bits) + bold, and the v2 upper
 * bits (a v1.4 binary masked the file token with 0x1f and drops these) */
#define	OF_STYLE 0x03		/* 0 solid, 1 dashed, 2 dotted            */
#define	OF_DASH	0x01
#define	OF_DOT	0x02
#define	OF_FILL	0x0c		/* 0 none, 4 white, 8 gray, 12 black      */
#define	OF_FILLW 0x04
#define	OF_FILLG 0x08
#define	OF_FILLB 0x0c
#define	OF_BOLD	0x10		/* line width 2                           */
#define	OF_HATCH 0x20		/* gray fill renders as 45-degree hatch   */
#define	OF_SMOOTH 0x40		/* polyline renders as a B-spline         */
#define	OF_VERT	0x80		/* text turned 90 degrees (glyph transpose) */

#define	MAXOBJ	400
#define	NAMEL	8
#define	VALL	16
#define	NLAYER	4		/* 0 drawing  1 annotation  2 frame       */
				/* 3 construction (never prints)          */

typedef struct {
	char	o_type;		/* OT_*                                   */
	char	o_sym;		/* OT_SYM: symbol index; OT_SHAPE: kind;  */
				/* OT_CONN: style; OT_POLY: point count   */
	char	o_rot;		/* OT_SYM: 0..3 quarter turns;            */
				/* OT_TEXT: size 0/1/2 (6x8/8x15/9x16)    */
	char	o_mir;		/* OT_SYM: 1 = mirrored                   */
	short	o_x, o_y;	/* grid: origin / first point / centre    */
	short	o_x2, o_y2;	/* grid: second point (circle: radius pt; */
				/* arc: o_x2 = radius; poly: o_x2 = ppool */
				/* base index)                            */
	char	o_flags;	/* OF_*                                   */
	char	o_layer;	/* 0..3                                   */
	char	o_grp;		/* persistent group id, 0 = none          */
	char	o_pad;		/* keeps o_name word-aligned (the ON()    */
				/* overlay below stores shorts there)     */
	char	o_name[NAMEL];	/* OT_SYM: designator ("" = none);        */
				/* OT_CONN: attachment shorts (ON());     */
				/* OT_ARC: a0/a1 shorts;  OT_NNAME: name  */
	char	o_val[VALL];	/* OT_SYM: value;  OT_TEXT: the text      */
				/* ('|' separates label-block lines);     */
				/* OT_SHAPE: the centred label            */
} DOBJ;

/* Connector attachments and arc angles live as SHORTS overlaid on o_name
 * (those types have no designator).  ACOBJ = object index (-1 = free
 * endpoint, -2 = "resolve from the stored grid point after load"),
 * ACPT = attachment-point index on it. */
#define	ON(o)		((short *)((o)->o_name))
#define	ACOBJ(o, e)	(ON(o)[(e) * 2])
#define	ACPT(o, e)	(ON(o)[(e) * 2 + 1])
#define	OA0(o)		(ON(o)[0])	/* arc start angle, degrees CCW */
#define	OA1(o)		(ON(o)[1])	/* arc end angle                */

extern DOBJ	obj[MAXOBJ];
extern int	nobj;

/* ---- canvas geometry constants (velbase's grid<->px transforms use
 * them; headless they are just constant offsets that cancel) ---- */
#define	GRID	8		/* px per grid unit at 1:1 zoom           */
#define	PALW	64		/* symbol palette width                   */
#define	CTOP	18		/* file-name bar height                   */
#define	TBH	20		/* toolbar row height                     */
#define	CANY	(CTOP + TBH)	/* canvas / palette top                   */
extern int	gsc;		/* px per grid unit ON SCREEN (4/8/16)    */

/* ---- junction dots ---- */
#define	MAXJUNC	128
extern short	juncx[MAXJUNC], juncy[MAXJUNC];
extern int	njunc;

/* ---- polyline point pool ---- */
#define	PPOOL	600
#define	PMAXPT	20
extern short	ppool[PPOOL];
extern int	ppuse;

/* ---- layers ---- */
extern int	curlayer;
extern char	layvis[NLAYER];
extern char	layprn[NLAYER];

/* ---- the one-level undo snapshot (vellum.c data -- editor-only, so
 * the headless exporter does not carry the 17 KB; velbase keeps only
 * the uvalid flag, which velfile clears on load) ---- */
extern DOBJ	uobj[MAXOBJ];
extern int	unobj;
extern short	uppool[PPOOL];
extern int	uppuse;

/* ---- the sheet (Settings presets; SHW/SHH read the live size) ---- */
extern int	v_shw, v_shh;
#define	SHW	v_shw
#define	SHH	v_shh

/* ---- sheet units ("U 5 mm": one grid unit is 5 mm) -- coordinates stay
 * grid units, ONLY dimension labels multiply ---- */
#define	UNAMEL	8
extern int	unum;
extern char	uname[UNAMEL];

/* ---- the symbol library (parsed from the library FILES) ---- */
#define	SEND	0
#define	SE	1
#define	SC	2
#define	ST	3
#define	SA	4		/* arc: cx cy r a0 a1 (degrees, y up)     */

typedef struct {
	char	*sy_code;	/* file code (and the status-line name)   */
	char	*sy_pfx;	/* designator prefix, "" = unnamed        */
	short	*sy_ops;
	short	*sy_pins;	/* count-prefixed q pairs (0 = none)      */
	char	sy_lib;		/* library group it belongs to            */
	short	sy_x0, sy_y0;	/* q bbox, computed once at start-up      */
	short	sy_x1, sy_y1;
} SYMDEF;

#define	MAXSYM	80		/* v2.4: pid/power/plan joined the set    */
#define	MAXLIB	10
#define	LIBLIST	"/usr/vellum/etc/libs"	/* start-up library list          */
#define	USERLIB	"/usr/vellum/etc/symbols" /* symedit's default scratch    */

extern SYMDEF	symtab[MAXSYM];
extern int	nsym;
extern char	libname[MAXLIB][12];
extern char	libpath[MAXLIB][44];
extern int	nlib;
extern int	curlib;

#define	SYMOPS	4800
#define	SYMPINS	600
extern short	symops[SYMOPS];
extern short	sympin[SYMPINS];
extern char	symcode[MAXSYM][8];
extern char	symprefix[MAXSYM][4];
extern int	opuse, pinuse;

/* Pin NAMES (netlists say Q1.B): pin PAIR i in sympin[] owns slot
 * (i+1)/2 in pinnm[] -- 0 = unnamed, else an offset into pnmpool. */
#define	PNMPOOL	640
extern char	pnmpool[PNMPOOL];
extern int	pnmuse;
extern short	pinnm[SYMPINS / 2];
#define	PINSLOT(s, k)	((int)((s)->sy_pins - sympin + 2 + 2 * (k)) / 2)

/* ---- shared state ---- */
extern int	modified;
extern int	voxg, voyg;	/* pan (loadfile homes it)                */
extern int	uvalid;		/* undo snapshot valid                    */
extern char	*shname[];	/* shape kind names, S lines              */
extern char	*csname[];	/* connector style names, K lines         */
extern char	osel[MAXOBJ];	/* the selection set                      */
extern int	nsel;
extern int	selobj;		/* primary selection (valid at nsel == 1) */
extern int	statdirty;
extern int	gridstep;	/* snap pitch, 1 or 2 units               */
#define	FNLEN	40
extern char	fname[];	/* current file name ("" = untitled)      */
extern int	mywid;		/* our window id (dialog event loops)     */
extern int	cursym;		/* armed palette symbol (-1 = none)       */
extern int	ddpal;		/* damage flag: whole palette bank        */
extern char	vbuf[], nbuf[];	/* the text/name dialog buffers           */

/* ---- veldlg.c: the dialogs ---- */
extern int	filedlg();	/* open/save file-name dialog             */
extern int	confirm();	/* "Discard unsaved changes?"             */
extern int	textdlg();	/* one text line -> vbuf                  */
extern int	propdlg();	/* selected object's properties           */
extern int	dohelp();
extern int	libdlg();	/* the scrollable library chooser         */
extern int	doedit();	/* open the current library in SymEdit    */
extern int	palview();	/* vellum.c: rebuild the palette view     */

/* ---- vellum.c services the command unit uses ---- */
extern int	dmg();		/* accumulate a canvas damage rect (px)   */
extern int	dmgobj();	/* damage object i + its labels/ring      */
extern int	dmgsel();	/* damage the whole selection             */
extern int	viewdirty();	/* whole-canvas view change               */
extern int	snapshot();	/* undo snapshot (before a commit)        */
extern int	moveobj();	/* moveobj(i, dx, dy) grid                */
extern int	objmove();	/* objmove(i, j): reorder, fix refs       */
extern int	objgbox();	/* grid-unit bbox of object i             */
extern int	delobj();	/* delete object i (fixes refs)           */
extern int	addobj();	/* append a plain object                  */
extern int	reroute();	/* pull attached connector endpoints      */
extern int	sheetsplit();	/* fname -> <pre><n><suf> sheet-set parts */

/* ---- velcmd.c: the editing commands ---- */
extern int	tofront(), toback();	/* z-order: selection to list ends */
extern int	dogroup(), doungroup();	/* persistent groups               */
extern int	doalign();	/* doalign(op): 1 L 2 R 3 T 4 B 5 dH 6 dV */
extern int	docopy(), docut();	/* selection -> hr clipboard (.d)  */
extern int	dopaste();	/* dopaste(gx, gy, clip): clip = 1 the
				 * clipboard, 0 the PRIMARY selection
				 * (falls back to the clipboard)          */
extern int	dostamp();	/* frame + title block onto layer 2       */
extern int	settingsdlg();	/* grid pitch / sheet / layers dialog     */
extern int	styledlg();	/* styledlg(i): style/fill/layer/text     */

/* ---- velbase.c: the model layer (cl_*-free geometry) ---- */
extern int	txq();		/* symbol-space q -> px transform         */
extern int	gtopx(), gtopy();	/* grid -> canvas px              */
extern int	gfloor(), gceil();	/* px -> grid floor/ceil          */
extern int	textdims();	/* longest '|' line + line count          */
extern int	sympbox();	/* symbol instance px bbox                */
extern int	objpbox2();	/* px bbox of a DOBJ (explicit pool)      */
extern int	objpbox();	/* px bbox of live object i               */
extern int	onwire();	/* grid point on a wire?                  */
extern int	addjunc();	/* junction-list append                   */
extern int	pinat();	/* symbol pin at grid point?              */
extern int	natt();		/* attachment-point count of object i     */
extern int	attpos();	/* attachment point k's grid position     */
extern int	dimlbl();	/* dimension label (auto: distance x unit) */
extern int	bspline();	/* chorded quadratic B-spline walker      */

/* ---- velgfx.c: pure drawing helpers (px + style in, cl_* out) ---- */
extern long	isqrt();	/* integer square root of a long          */
extern int	isin(), icos();	/* sin/cos(deg) scaled by 256             */
extern int	iangle();	/* iangle(dx, dy): degrees 0..359, y up   */
extern int	arcline();	/* chorded arc (solid)                    */
extern int	flatarc();	/* shallow bulge arc (drum/doc)           */
extern int	stpat();	/* set cl_lpat from OF_STYLE              */
extern int	sline();	/* styled line (+OF_BOLD)                 */
extern int	rowspan();	/* one fill span; val 4 = 45-degree hatch */
extern int	fillval();	/* OF_FILL/OF_HATCH -> rowspan val        */
extern int	drawspline();	/* smooth polyline: B-spline chords       */
extern int	arcst();	/* styled arc                             */
extern int	scirc();	/* styled circle                          */
extern int	fontslot();	/* text size 0/1/2 -> SHM_F* slot         */
extern int	fillshape();	/* row-span fill of a shape kind          */
extern int	shapeoutline();	/* parametric outline of a shape kind     */
extern int	shlabel();	/* centred, truncated shape label         */
extern int	arrowhead();	/* connector arrow barbs                  */
extern int	dimdraw();	/* dimension: ticks + arrows + label      */
extern int	vtext();	/* vertical (transposed) text via cl_blit */

/* ---- cross-unit functions ---- */
extern char	*tok();		/* velfile.c: tokenizer                   */
extern int	symbycode();	/* velfile.c                              */
extern int	loadlib();	/* velfile.c: one .sym library file       */
extern int	loadsyms();	/* velfile.c: LIBLIST + user scratch      */
extern int	fmtobj();	/* velfile.c: object -> .d line           */
extern int	writefile();	/* velfile.c: save, modified untouched    */
extern int	savefile();	/* velfile.c: save + clear modified       */
extern int	parsereset();	/* velfile.c: reset the line parser       */
extern int	parseobj();	/* velfile.c: one .d line -> obj[nobj]    */
extern int	loadfile();	/* velfile.c: whole file                  */
extern int	newgid();	/* velfile.c: first free group id         */
extern int	selclear();	/* vellum.c                               */
extern int	rejunc();	/* vellum.c: junction dots                */
extern int	symbounds();	/* vellum.c: symbol q bboxes              */
extern int	resolveatt();	/* vellum.c: re-resolve one conn end      */
extern int	velxport();	/* velport.c: headless -print/-pic/-net   */
