#!/usr/bin/env python3
"""Generate the hrgui system font files (/usr/hr/fonts/*.hf) that the window
server loads into the shared VRAM tail (inc/shmem.h).  A single copy of each
lives in the tail; the server AND every direct-render client blit glyphs from
there -- no relink into a client, no per-glyph kernel trap.

Three fonts (all monospaced, so a simple fixed-cell format suffices):
  gallant.hf  12x22  terminal content   (from the kernel gall.c glyph table)
  gacha.hf     9x16  system / UI chrome (from FM font gacha.b.8, GUI.md sysfont)
  sail.hf      6x8   minimized-icon labels (from FM font sail.r.6)

The GUI terminal uses a 22px cell, not the kernel console's 25px one: the kernel
gall.c glyph has 5 blank rows of top leading, and the GUI drops 3 of them so the
window terminal packs more lines per screen.  All 20 inked rows (5..24) and the
bottom descenders are kept intact; only blank top-leading rows are removed.

.hf format (big-endian 16-bit words == Z8001 native int order):
    word0 = first_char     (0x20)
    word1 = nchars         (95: 0x20..0x7e)
    word2 = cellw          (glyph width px, <= 16)
    word3 = cellh          (glyph height px == words per glyph)
    then nchars*cellh glyph words; bit15 = leftmost pixel, set = INK.  Ink=1 is
    normalized here for every font so the one renderer (blit L_NSRC -> black ink
    on a white cell) works for all of them, matching srvicon and the reference.
"""
import os, re, struct, sys

HERE   = os.path.dirname(os.path.abspath(__file__))
GALLC  = os.path.join(HERE, "..", "src", "kernel", "z8001", "drv", "hrtty", "gall.c")
FMDIR  = os.path.join(HERE, "..", "build", "root", "usr", "hr", "fonts")
OUTDIR = os.path.join(HERE, "..", "build", "root", "usr", "hr", "fonts")

FIRST = 0x20
NCH   = 95        # 0x20..0x7e inclusive (all printable ASCII)
GALLH = 22        # GUI terminal cell height (kernel gall.c is 25; drop 3 blank
                  # top-leading rows, keeping all 20 inked rows 5..24)

# --- source A: the kernel's fixed 12x25 gallant table (gall.c) -------------
def glyphs_from_gall():
    text = open(GALLC).read()
    text = text[text.index("font[][25]"):]
    words = [int(w, 16) for w in re.findall(r"0x[0-9a-fA-F]{1,4}", text)]
    cellh = 25
    need = NCH * cellh
    if len(words) < need:
        sys.exit("gall.c: %d words, need %d" % (len(words), need))
    # gall.c glyphs already carry the 12px in bits15..4, ink=1.
    glyphs = [words[g*cellh:(g+1)*cellh] for g in range(NCH)]
    # Trim to the GUI cell by dropping leading rows from the top -- assert they
    # are blank across every glyph so no ink is ever clipped (only leading).
    trim = cellh - GALLH
    for rows in glyphs:
        for y in range(trim):
            if rows[y] & 0xFFF0:
                sys.exit("gall.c: row %d is not blank; cannot trim to %d" % (y, GALLH))
    return 12, GALLH, [rows[trim:] for rows in glyphs]

# --- source B: an FM font file (f2.c FONT_HEADER + loctable + kwtable + strip)
def glyphs_from_fm(name):
    d = open(os.path.join(FMDIR, name), "rb").read()
    first, last = d[0], d[1]
    asc, desc, lead, maxw, minw = struct.unpack("bbbbb", d[2:7])
    if maxw != minw:
        sys.exit("%s: proportional (maxw=%d minw=%d); .hf is fixed-cell only"
                 % (name, maxw, minw))
    off = 22
    cornx, corny = struct.unpack(">hh", d[off:off+4]); off += 4
    tbl = last - first + 3
    loct = struct.unpack(">%dh" % tbl, d[off:off+tbl*2]); off += tbl*2
    off += tbl*2                                  # skip kwtable
    mapw = ((cornx + 15) // 16) * 16
    fwords = (mapw // 16) * corny
    bmp = struct.unpack(">%dH" % fwords, d[off:off+fwords*2])
    wpr = mapw // 16
    def px(x, y):
        return (bmp[y*wpr + (x >> 4)] >> (15 - (x & 15))) & 1
    cellw, cellh = maxw, corny
    out = []
    for c in range(FIRST, FIRST + NCH):
        g = c - first
        x0 = loct[g]
        rows = []
        for y in range(cellh):
            w = 0
            for xx in range(cellw):
                if px(x0 + xx, y):
                    w |= 0x8000 >> xx        # bit15 = leftmost, ink into top bits
            rows.append(w)
        out.append(rows)
    return cellw, cellh, out

def normalize_ink(cellw, glyphs):
    """Force ink=1 (sparse): if >50% of cell bits are set, the source stored
    background=1, so invert every glyph within the cell mask."""
    mask = (0xFFFF << (16 - cellw)) & 0xFFFF
    set_bits = tot = 0
    for rows in glyphs:
        for w in rows:
            for b in range(cellw):
                tot += 1
                set_bits += (w >> (15 - b)) & 1
    if set_bits > tot * 0.5:
        glyphs = [[(~w) & mask for w in rows] for rows in glyphs]
    return glyphs

def write_hf(base, cellw, cellh, glyphs):
    flat = [w for rows in glyphs for w in rows]
    os.makedirs(OUTDIR, exist_ok=True)
    with open(os.path.join(OUTDIR, base), "wb") as f:
        f.write(struct.pack(">HHHH", FIRST, NCH, cellw, cellh))
        f.write(struct.pack(">%dH" % len(flat), *flat))
    print("wrote %-11s %2dx%-2d %d glyphs (%d bytes)"
          % (base, cellw, cellh, NCH, 8 + len(flat)*2))

def preview(path, specs):
    try:
        from PIL import Image
    except ImportError:
        return
    SC, pad = 3, 6
    line = "Terminal #1 Clock ABCabcg 0123"
    imgs = []
    for base, cellw, cellh, glyphs in specs:
        W = len(line) * cellw
        buf = [[0]*W for _ in range(cellh)]
        for i, ch in enumerate(line):
            c = ord(ch)
            if FIRST <= c < FIRST + NCH:
                rows = glyphs[c - FIRST]
                for y in range(cellh):
                    for x in range(cellw):
                        if (rows[y] >> (15 - x)) & 1:
                            buf[y][i*cellw + x] = 1
        imgs.append((base, buf, W, cellh))
    from PIL import Image
    tot_h = sum(h*SC + pad for _, _, _, h in imgs) + pad
    tot_w = max(w*SC for _, _, w, _ in imgs) + 2*pad
    img = Image.new("L", (tot_w, tot_h), 128); px = img.load()
    y = pad
    for base, buf, W, h in imgs:
        for yy in range(h):
            for xx in range(W):
                v = 0 if buf[yy][xx] else 255      # ink=1 -> black
                for sy in range(SC):
                    for sx in range(SC):
                        px[pad+xx*SC+sx, y+yy*SC+sy] = v
        y += h*SC + pad
    img.save(path); print("preview ->", path)

def main():
    specs = []
    cw, ch, g = glyphs_from_gall();          specs.append(("gallant.hf", cw, ch, normalize_ink(cw, g)))
    cw, ch, g = glyphs_from_fm("gacha.b.8"); specs.append(("gacha.hf",  cw, ch, normalize_ink(cw, g)))
    cw, ch, g = glyphs_from_fm("sail.r.6");  specs.append(("sail.hf",   cw, ch, normalize_ink(cw, g)))
    for base, cw, ch, g in specs:
        write_hf(base, cw, ch, g)
    if len(sys.argv) > 1:
        preview(sys.argv[1], specs)

if __name__ == "__main__":
    main()
