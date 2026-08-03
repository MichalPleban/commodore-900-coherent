#!/usr/bin/env python3
"""Generate the hrgui system font files (/usr/hr/fonts/*.hf) that the window
server loads into the shared VRAM tail (inc/shmem.h).  A single copy of each
lives in the tail; the server AND every direct-render client blit glyphs from
there -- no relink into a client, no per-glyph kernel trap.

Three fonts (all monospaced, so a simple fixed-cell format suffices):
  gacha.r.hf   8x15  terminal content   (from FM font gacha.r.7)
  gacha.b.hf   9x16  system / UI chrome (from FM font gacha.b.8, GUI.md sysfont)
  sail.hf      6x8   minimized-icon labels (from FM font sail.r.6)

The terminal font is the 8px-wide gacha REGULAR, not the 12px gallant the kernel
console uses: an 8px cell is exactly one byte, and a window's content origin is
16-aligned, so every character cell starts on a byte boundary -- glyph output is
a plain byte store instead of a shift-and-mask blit, and 1024/8 = 128 columns fit
across the screen.  gacha bold is the same glyphs smeared 1px right (b = r | r>>1),
which is why the chrome font is 9 wide; keeping regular for content also gives
window chrome a visible weight contrast against terminal text.

.hf format (big-endian 16-bit words == Z8001 native int order):
    word0 = first_char     (0x20)
    word1 = nchars         (95: 0x20..0x7e)
    word2 = cellw          (glyph width px, <= 16)
    word3 = cellh          (glyph height px == words per glyph)
    then nchars*cellh glyph words; bit15 = leftmost pixel, set = INK.  Ink=1 is
    normalized here for every font so the one renderer (blit L_NSRC -> black ink
    on a white cell) works for all of them, matching srvicon and the reference.
"""
import os, struct, sys

HERE   = os.path.dirname(os.path.abspath(__file__))
FMDIR  = os.path.join(HERE, "..", "build", "root", "usr", "hr", "fonts")
OUTDIR = os.path.join(HERE, "..", "build", "root", "usr", "hr", "fonts")

FIRST = 0x20
NCH   = 95        # 0x20..0x7e inclusive (all printable ASCII)

# --- source: an FM font file (f2.c FONT_HEADER + loctable + kwtable + strip)
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
    cw, ch, g = glyphs_from_fm("gacha.r.7"); specs.append(("gacha.r.hf", cw, ch, normalize_ink(cw, g)))
    cw, ch, g = glyphs_from_fm("gacha.b.8"); specs.append(("gacha.b.hf", cw, ch, normalize_ink(cw, g)))
    cw, ch, g = glyphs_from_fm("sail.r.6");  specs.append(("sail.hf",    cw, ch, normalize_ink(cw, g)))
    for base, cw, ch, g in specs:
        write_hf(base, cw, ch, g)
    if len(sys.argv) > 1:
        preview(sys.argv[1], specs)

if __name__ == "__main__":
    main()
