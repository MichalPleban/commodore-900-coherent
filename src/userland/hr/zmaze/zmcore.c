/*
 * zmcore.c - zmaze render core: raycaster, tables, map, movement.
 *
 * GUI-independent: renders one first-person frame of the 16x16 maze into
 * frame[] (a W x H 1bpp image, BPR bytes per row).  The front end (zmaze.c,
 * a zview client) decides when to render and how frame[] reaches the
 * screen; a benchmark harness can link this core with its own main.
 *
 * All arithmetic is 8.8 fixed point: positions in map cells (1 cell = 256),
 * angles 0..255 per circle.  The Z8001's divl costs ~720 cycles, so the
 * render path never divides: recip[] serves the DDA setup, hgt[] the
 * wall-height projection, camx[] the camera plane.  The hot inner loops
 * live in zmaze_a.s (see there): castray (DDA), mixrows (ragged-edge row
 * compositor), colfill (solid vertical dither bands).
 */

#define W	320		/* viewport size */
#define H	200
#define BPR	(W/8)		/* viewport bytes per row */

#define MAPSH	4		/* 16x16 map */
#define MAPW	(1<<MAPSH)
#define FIX	8		/* 8.8 fixed point */
#define ONE	(1<<FIX)

#define INF	0x7fff

extern colfill();		/* zmaze_a.s: strided pattern column fill  */
extern castray();		/* zmaze_a.s: DDA raycast -> perp distance */
extern castcol();		/* zmaze_a.s: all 8 rays of a byte column  */
extern mixrows();		/* zmaze_a.s: per-pixel edge row compositor */

static char *mapsrc[MAPW] = {
	"################",
	"#..............#",
	"#..##..####..#.#",
	"#..##..#.....#.#",
	"#..............#",
	"####..##..##...#",
	"#......#....#..#",
	"#..##..#..###..#",
	"#..##..#.......#",
	"#...........####",
	"#..#####.......#",
	"#..#...#..##...#",
	"#..#.#.#..##...#",
	"#....#.........#",
	"#..###....##...#",
	"################",
};

/* Flat 16x16 wall map (1 = wall), row-major: castray's asm DDA walks it
 * with a single pointer (x step = +-1, y step = +-16), and testb decides. */
char	flatmap[MAPW * MAPW];

/* 8.8 sine, 256 steps per circle */
static int sintab[256] = {
	0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92,
	98, 104, 109, 115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177,
	181, 185, 190, 194, 198, 202, 206, 209, 213, 216, 220, 223, 226, 229, 231, 234,
	237, 239, 241, 243, 245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 256, 256,
	256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245, 243, 241, 239,
	237, 234, 231, 229, 226, 223, 220, 216, 213, 209, 206, 202, 198, 194, 190, 185,
	181, 177, 172, 167, 162, 157, 152, 147, 142, 137, 132, 126, 121, 115, 109, 104,
	98, 92, 86, 80, 74, 68, 62, 56, 50, 44, 38, 31, 25, 19, 13, 6,
	0, -6, -13, -19, -25, -31, -38, -44, -50, -56, -62, -68, -74, -80, -86, -92,
	-98, -104, -109, -115, -121, -126, -132, -137, -142, -147, -152, -157, -162, -167, -172, -177,
	-181, -185, -190, -194, -198, -202, -206, -209, -213, -216, -220, -223, -226, -229, -231, -234,
	-237, -239, -241, -243, -245, -247, -248, -250, -251, -252, -253, -254, -255, -255, -256, -256,
	-256, -256, -256, -255, -255, -254, -253, -252, -251, -250, -248, -247, -245, -243, -241, -239,
	-237, -234, -231, -229, -226, -223, -220, -216, -213, -209, -206, -202, -198, -194, -190, -185,
	-181, -177, -172, -167, -162, -157, -152, -147, -142, -137, -132, -126, -121, -115, -109, -104,
	-98, -92, -86, -80, -74, -68, -62, -56, -50, -44, -38, -31, -25, -19, -13, -6,
};
#define SIN(a)	sintab[(a) & 255]
#define COS(a)	sintab[((a) + 64) & 255]

/* Distance-band wall dither, light (near) to dark (far); each byte repeats
 * a 4-pixel pattern so full-byte writes stay aligned.  Bit set = white.
 * Rows indexed by absolute buffer y & 3.  EIGHT levels a step of ~6-12%
 * apart: the level is the half-cell distance band (perp>>7, capped at 3)
 * DOUBLED, plus one for walls faced across x -- so the side penalty at a
 * corner is a gentle half-step, where a full band of the old 5-level
 * scale could put 75% against 37% between two walls at nearly the same
 * distance.  The half-cell distance plateaus are kept wide on purpose:
 * they are what lets colfill's solid fast path (same shade across a byte
 * column) carry most of the wall area.  Walls stay >= 25% against the
 * black ceiling and sparse (12%) floor. */
static char wallpat[8][4] = {
	{ 0xff, 0xdd, 0xff, 0x77 },	/* 0: 87% */
	{ 0x77, 0xdd, 0x77, 0xdd },	/* 1: 75% */
	{ 0xee, 0x55, 0xbb, 0x55 },	/* 2: 62% */
	{ 0xaa, 0x55, 0xaa, 0x55 },	/* 3: 50% */
	{ 0xaa, 0x55, 0xaa, 0x11 },	/* 4: 44% */
	{ 0xaa, 0x22, 0x55, 0x88 },	/* 5: 37% */
	{ 0x22, 0x55, 0x88, 0x11 },	/* 6: 31% */
	{ 0x22, 0x88, 0x22, 0x88 },	/* 7: 25% */
};
char	floorpat[4] = { 0x44, 0x00, 0x11, 0x00 };

int	frame[H * BPR / 2];	/* int-aligned 1bpp viewport image */

/* Render-path tables (see header comment; hgt/camx are read by castcol_). */
int	recip[512];
int	hgt[1024];
int	camx[W];
int	hgt2[1024];		/* like hgt but UNCLAMPED (BSP interpolates it) */

int	px, py;			/* player position, 8.8 map cells */
int	pang;			/* player angle, 0..255 */

static char phz[4] = { 0, 0, 0, 0 };	/* black fill for colfill */

/* per-byte-column ray results, written by castcol_, read by mixrows_ */
int	rtop[8], rbot[8];
int	rpat[8];
/* per-column edge patterns, phase-major: colpb[y&3][k], read by mixrows_ */
char	colpb[4][8];
/* per-frame ray-setup constants and castcol_'s per-column reductions */
int	cdrx, cdry;		/* view direction, 8.8 */
int	cplx, cply;		/* camera plane, 8.8 */
int	cres[4];		/* t0, t1, b0, b1 */
int	csame;			/* all 8 rays share one shade band */

/* ---- BSP wall-segment renderer (rmode 1; the raycaster is rmode 0, ----
 * ---- kept whole as the fallback -- the r key flips live) --------------
 *
 * Instead of marching 320 independent rays, walk a BSP over the maze's
 * wall FACES front-to-back from the eye and project each visible segment
 * once: for a straight wall 1/z -- and so the projected column height --
 * is LINEAR in screen x, so the whole span is one fixed-point add per
 * column.  A column occlusion buffer keeps the nearest writer and stops
 * the walk when all 320 columns are filled.  Output goes to ctop/cbot/
 * cpat[320]; the drawing back half (colfill/mixrows staging) is shared
 * with the raycaster unchanged. */

int	rmode = 1;		/* 1 = BSP segments, 0 = raycast */

#define MAXSEG	256
#define NEARZ	16		/* near clip, 8.8 cells */

struct wseg {
	char	vert;		/* 1: on line x = c;  0: on line y = c  */
	char	face;		/* empty side: +1 above/right of c, -1 below/left */
	char	c, a, b;	/* line coord and span [a,b), cell units */
};
static struct wseg segs[MAXSEG];
static int	nsegs;

/* BSP nodes: splitter line + the co-linear segs + two children. */
#define MAXNODE	128
static char	nvert[MAXNODE];
static char	nc[MAXNODE];
static int	nseg0[MAXNODE], nsegn[MAXNODE];	/* slice of onpool[] */
static int	nlo[MAXNODE], nhi[MAXNODE];	/* children, -1 = leaf side */
static int	nnodes;
static int	onpool[MAXSEG];
static int	nonpool;

int	ctop[W], cbot[W], cpat[W];	/* read by colmm_ / written by emitspan_ */
char	cfill[W];
int	colsleft;
long	dhw;			/* emitspan_'s parked span slope */

extern emitspan();		/* zmaze_a.s: span inner loop */
extern colmm();			/* zmaze_a.s: column copy + reductions */

extern char	*malloc();

static
addseg(vert, face, c, a, b)
{
	register struct wseg *s;

	if (nsegs >= MAXSEG)
		return 0;
	s = &segs[nsegs++];
	s->vert = vert;
	s->face = face;
	s->c = c;
	s->a = a;
	s->b = b;
	return 0;
}

/* Pull the maze's directed wall faces (empty cell against wall cell),
 * merging colinear runs with the same facing. */
static
mksegs()
{
	register int i, j;
	int f, run, rf;

	nsegs = 0;
	for (i = 1; i < MAPW; i++) {		/* vertical faces on x = i */
		run = -1;
		rf = 0;
		for (j = 0; j <= MAPW; j++) {
			f = 0;
			if (j < MAPW) {
				if (flatmap[(j << MAPSH) + i - 1] &&
				    !flatmap[(j << MAPSH) + i])
					f = 1;	/* empty at x > i */
				else if (!flatmap[(j << MAPSH) + i - 1] &&
					 flatmap[(j << MAPSH) + i])
					f = -1;	/* empty at x < i */
			}
			if (f != rf) {
				if (rf)
					addseg(1, rf, i, run, j);
				run = j;
				rf = f;
			}
		}
	}
	for (i = 1; i < MAPW; i++) {		/* horizontal faces on y = i */
		run = -1;
		rf = 0;
		for (j = 0; j <= MAPW; j++) {
			f = 0;
			if (j < MAPW) {
				if (flatmap[((i-1) << MAPSH) + j] &&
				    !flatmap[(i << MAPSH) + j])
					f = 1;	/* empty at y > i */
				else if (!flatmap[((i-1) << MAPSH) + j] &&
					 flatmap[(i << MAPSH) + j])
					f = -1;
			}
			if (f != rf) {
				if (rf)
					addseg(0, rf, i, run, j);
				run = j;
				rf = f;
			}
		}
	}
	return 0;
}

/* Recursive builder: splitter = the first seg's line; parallel segs sort
 * by side (co-linear ones join the node), perpendicular segs crossing the
 * line are split at it (always on a whole cell coordinate). */
static int
bspbuild(list, n)
int *list;
{
	register struct wseg *s;
	int *lo, *hi;
	int nl, nh, i, nd, v, c, si;

	if (n <= 0)
		return -1;
	if (nnodes >= MAXNODE)
		return -1;
	nd = nnodes++;
	v = segs[list[0]].vert;
	c = segs[list[0]].c;
	nvert[nd] = v;
	nc[nd] = c;
	nseg0[nd] = nonpool;
	lo = (int *)malloc(n * sizeof(int));
	hi = (int *)malloc(n * sizeof(int));
	nl = nh = 0;
	for (i = 0; i < n; i++) {
		si = list[i];
		s = &segs[si];
		if (s->vert == v) {
			if (s->c == c)
				onpool[nonpool++] = si;
			else if (s->c > c)
				hi[nh++] = si;
			else
				lo[nl++] = si;
		} else {
			if (s->b <= c)
				lo[nl++] = si;
			else if (s->a >= c)
				hi[nh++] = si;
			else {			/* split at the line */
				addseg(s->vert, s->face, s->c, c, s->b);
				s->b = c;
				lo[nl++] = si;
				hi[nh++] = nsegs - 1;
			}
		}
	}
	nsegn[nd] = nonpool - nseg0[nd];
	nlo[nd] = bspbuild(lo, nl);
	nhi[nd] = bspbuild(hi, nh);
	free(lo);
	free(hi);
	return nd;
}

static int	bsproot;

/* Project + emit one wall segment into the column buffers. */
static
emitseg(s)
register struct wseg *s;
{
	int wx0, wy0, wx1, wy1;
	int rx, ry, z0, z1, w0, w1, t;
	int x0, x1, hh0, hh1, sh, h, hh;
	register int x;
	long sxl, dh, hacc;

	if (s->vert) {
		wx0 = wx1 = s->c << 8;
		wy0 = s->a << 8;
		wy1 = s->b << 8;
	} else {
		wy0 = wy1 = s->c << 8;
		wx0 = s->a << 8;
		wx1 = s->b << 8;
	}
	rx = wx0 - px;  ry = wy0 - py;
	z0 = (int)(((long)rx * cdrx + (long)ry * cdry) >> 8);
	w0 = (int)(((long)ry * cdrx - (long)rx * cdry) >> 8);
	rx = wx1 - px;  ry = wy1 - py;
	z1 = (int)(((long)rx * cdrx + (long)ry * cdry) >> 8);
	w1 = (int)(((long)ry * cdrx - (long)rx * cdry) >> 8);

	if (z0 < NEARZ && z1 < NEARZ)
		return 0;
	if (z0 < NEARZ) {			/* clip p0 to the near plane */
		t = (int)(((long)(NEARZ - z0) << 8) / (z1 - z0));
		w0 += (int)(((long)(w1 - w0) * t) >> 8);
		z0 = NEARZ;
	} else if (z1 < NEARZ) {
		t = (int)(((long)(NEARZ - z1) << 8) / (z0 - z1));
		w1 += (int)(((long)(w0 - w1) * t) >> 8);
		z1 = NEARZ;
	}

	sxl = (long)w0 * 479 / z0;
	if (sxl > 1000) sxl = 1000;
	if (sxl < -1000) sxl = -1000;
	x0 = ((int)sxl + (W - 1)) >> 1;
	sxl = (long)w1 * 479 / z1;
	if (sxl > 1000) sxl = 1000;
	if (sxl < -1000) sxl = -1000;
	x1 = ((int)sxl + (W - 1)) >> 1;

	if (z0 < 24) z0 = 24;  else if (z0 > 4092) z0 = 4092;
	if (z1 < 24) z1 = 24;  else if (z1 > 4092) z1 = 4092;
	hh0 = hgt2[z0 >> 2];
	hh1 = hgt2[z1 >> 2];
	if (x1 < x0) {
		t = x0;  x0 = x1;  x1 = t;
		t = hh0;  hh0 = hh1;  hh1 = t;
	}
	if (x1 < 0 || x0 >= W)
		return 0;
	dh = 0;
	if (x1 > x0)
		dh = ((long)(hh1 - hh0) << 8) / (x1 - x0);
	hacc = (long)hh0 << 8;
	if (x0 < 0) {
		hacc -= dh * x0;
		x0 = 0;
	}
	if (x1 >= W)
		x1 = W - 1;
	if (x1 >= x0)
		emitspan(x0, x1, hacc, dh, s->vert ? 1 : 0);
	return 0;
}

/* Front-to-back walk: near child, then the facing co-linear segs, then
 * the far child; stop as soon as every column has its nearest wall. */
static
bspwalk(nd)
{
	int eyec, i, hi1st;

	if (nd < 0 || colsleft <= 0)
		return 0;
	eyec = nvert[nd] ? px : py;
	hi1st = eyec >= (nc[nd] << 8);
	bspwalk(hi1st ? nhi[nd] : nlo[nd]);
	if (colsleft > 0)
		for (i = 0; i < nsegn[nd]; i++) {
			register struct wseg *s = &segs[onpool[nseg0[nd] + i]];

			if (eyec > (s->c << 8) ? s->face > 0 : s->face < 0)
				emitseg(s);
		}
	bspwalk(hi1st ? nlo[nd] : nhi[nd]);
	return 0;
}

static
visbsp()
{
	register int x;

	for (x = 0; x < W; x++)
		cfill[x] = 0;
	colsleft = W;
	bspwalk(bsproot);
	if (colsleft > 0)			/* should not happen: closed maze */
		for (x = 0; x < W; x++)
			if (!cfill[x]) {
				ctop[x] = (H - 2) >> 1;
				cbot[x] = (H + 2) >> 1;
				cpat[x] = 3;
			}
	return 0;
}

inittab()
{
	register int i;
	register char *m;
	long t;

	recip[0] = INF;
	for (i = 1; i < 512; i++) {
		t = 65536L / i;
		recip[i] = t > INF ? INF : (int)t;
	}
	hgt2[0] = hgt2[1] = hgt2[2] = hgt2[3] = hgt2[4] = hgt2[5] = 2133;
	for (i = 6; i < 1024; i++) {
		t = 12800L / i;		/* 51200 / (i<<2) */
		hgt2[i] = (int)t;
		hgt[i] = t > H ? H : (int)t;
	}
	hgt[0] = hgt[1] = hgt[2] = hgt[3] = hgt[4] = hgt[5] = H;
	for (i = 0; i < W; i++)
		camx[i] = (int)(((long)(2 * i - W + 1) << FIX) / W);
	m = flatmap;
	for (i = 0; i < MAPW * MAPW; i++)
		*m++ = mapsrc[i >> MAPSH][i & (MAPW-1)] != '.';

	/* wall faces + the BSP over them, for the rmode 1 renderer */
	mksegs();
	{
		int list[MAXSEG];

		nnodes = 0;
		nonpool = 0;
		for (i = 0; i < nsegs; i++)
			list[i] = i;
		bsproot = bspbuild(list, nsegs);
	}

	px = (3 << FIX) / 2;		/* start at (1.5, 1.5) */
	py = (3 << FIX) / 2;
	pang = 32;			/* facing into the maze */
	return 0;
}

static int
mapat(x, y)
{
	if (x < 0 || x >= MAPW || y < 0 || y >= MAPW)
		return 1;
	return flatmap[(y << MAPSH) + x];
}

static
move(dx, dy)
int dx, dy;
{
	int nx, ny;

	nx = px + dx;
	ny = py + dy;
	/* axis-separate collision so we slide along walls */
	if (!mapat(nx >> FIX, py >> FIX))
		px = nx;
	if (!mapat(px >> FIX, ny >> FIX))
		py = ny;
	return 0;
}

/* One game key; returns 1 if the view changed (caller re-renders).
 * The cursor keys arrive as the MicroEMACS control codes the zvpump
 * keymap emits for the nav block (Up ^P  Down ^N  Left ^B  Right ^F --
 * see zvpump.c), so arrows work alongside w/a/s/d with no server help. */
dokey(c)
{
	switch (c) {
	case 'w':
	case 'P' & 0x1f:		/* ^P = Up */
		move((int)(((long)COS(pang) * 64) >> FIX),
		     (int)(((long)SIN(pang) * 64) >> FIX));
		return 1;
	case 's':
	case 'N' & 0x1f:		/* ^N = Down */
		move((int)(((long)-COS(pang) * 64) >> FIX),
		     (int)(((long)-SIN(pang) * 64) >> FIX));
		return 1;
	case 'a':
	case 'B' & 0x1f:		/* ^B = Left */
		pang = (pang - 8) & 255;
		return 1;
	case 'd':
	case 'F' & 0x1f:		/* ^F = Right */
		pang = (pang + 8) & 255;
		return 1;
	case 'r':			/* flip renderer: BSP <-> raycast */
		rmode ^= 1;
		return 1;
	}
	return 0;
}

render()
{
	register char *fp;
	register int y;
	int bx, k;
	int t0, t1, b0, b1, same;
	char ph[4];

	cdrx = COS(pang);
	cdry = SIN(pang);
	cplx = (int)(((long)-cdry * 171) >> FIX);	/* 0.66 FOV */
	cply = (int)(((long)cdrx * 171) >> FIX);

	if (rmode)
		visbsp();		/* fills ctop/cbot/cpat[320] */

	for (bx = 0; bx < BPR; bx++) {
		if (rmode)
			colmm(bx);	/* copy + reductions, zmaze_a.s */
		else
			castcol(bx);	/* rays + reductions, zmaze_a.s */
		t0 = cres[0];  t1 = cres[1];
		b0 = cres[2];  b1 = cres[3];
		same = csame;

		/* ragged rows exist: stage each column's dither bytes where
		 * mixrows_ reads them at fixed addresses (colpb[y&3][k]) */
		if (t1 > t0 || b1 > b0 || !same)
			for (k = 0; k < 8; k++) {
				register char *w = wallpat[rpat[k]];
				colpb[0][k] = w[0];
				colpb[1][k] = w[1];
				colpb[2][k] = w[2];
				colpb[3][k] = w[3];
			}

		fp = (char *)frame + bx;
		if (t0 > 0)
			colfill(fp, phz, t0);		/* ceiling */
		y = t0;
		fp += y * BPR;
		if (t1 > y) {
			mixrows(fp, y, t1);		/* ragged top edge */
			fp += (t1 - y) * BPR;
			y = t1;
		}
		if (same) {
			if (b0 > y) {
				register char *pat = wallpat[rpat[0]];
				for (k = 0; k < 4; k++)
					ph[k] = pat[(y + k) & 3];
				colfill(fp, ph, b0 - y);   /* solid wall band */
				fp += (b0 - y) * BPR;
				y = b0;
			}
		} else if (b0 > y) {
			mixrows(fp, y, b0);	/* mixed-shade wall band */
			fp += (b0 - y) * BPR;
			y = b0;
		}
		if (b1 > y) {
			mixrows(fp, y, b1);		/* ragged bottom edge */
			fp += (b1 - y) * BPR;
			y = b1;
		}
		if (y < H) {
			for (k = 0; k < 4; k++)
				ph[k] = floorpat[(y + k) & 3];
			colfill(fp, ph, H - y);		/* floor */
		}
	}
	return 0;
}
