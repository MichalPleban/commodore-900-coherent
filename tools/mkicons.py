#!/usr/bin/env python3
"""Extract the 13 glyphs of the original icons48 font into standalone .icn files
for the hrgui desktop icons, and render a preview PNG so the right glyphs can be
chosen for each app.

.icn format (big-endian 16-bit words, matching the framebuffer word order):
    word0 = width  (pixels)
    word1 = height (pixels)
    then height rows, each ceil(width/16) words; bit15 = leftmost pixel,
    bit set = white (background), bit clear = black (stroke) -- i.e. the raw
    icons48 bits, so a plain L_SRC blit paints a white cell with black artwork.

icons48 layout (decoded): FONT_HEADER 26 bytes, loctable, kwtable, then the
bitmap at file offset 86 -- a 640px-wide (40 words/row) x 48-row strip; icon N
(codes 0x20..0x2c) is 48px wide starting at column N*48 = word N*3.
"""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC  = os.path.join(HERE, "..", "_graphics", "hr", "fonts", "icons48")
OUTDIR = os.path.join(HERE, "..", "build", "root", "usr", "hr", "icons")
SRCDIR = os.path.join(HERE, "..", "src", "userland", "hr", "icons")

BITMAP_OFF = 86
STRIP_WORDS = 40          # 640 px / 16
H = 48
IW = 48                   # icon width
IWORDS = IW // 16 if IW % 16 == 0 else IW // 16 + 1   # = 3
NICONS = 13

def load_strip():
    with open(SRC, "rb") as f:
        data = f.read()
    # big-endian words for the whole strip
    nwords = STRIP_WORDS * H
    words = struct.unpack(">%dH" % nwords, data[BITMAP_OFF:BITMAP_OFF + nwords * 2])
    return words

def icon_rows(words, n):
    """Return H rows, each IWORDS words, for icon n."""
    rows = []
    for r in range(H):
        base = r * STRIP_WORDS + n * IWORDS
        rows.append(words[base:base + IWORDS])
    return rows

def write_icn(path, rows):
    with open(path, "wb") as f:
        f.write(struct.pack(">HH", IW, H))
        for row in rows:
            f.write(struct.pack(">%dH" % IWORDS, *row))

def preview(words, path):
    try:
        from PIL import Image
    except ImportError:
        return
    pad = 8
    W = NICONS * (IW + pad) + pad
    img = Image.new("L", (W, H + 2 * pad), 128)
    px = img.load()
    for n in range(NICONS):
        rows = icon_rows(words, n)
        ox = pad + n * (IW + pad)
        for r in range(H):
            for c in range(IW):
                w = rows[r][c // 16]
                bit = (w >> (15 - (c % 16))) & 1
                px[ox + c, pad + r] = 255 if bit else 0   # 1=white
    img = img.resize((W * 2, (H + 2 * pad) * 2), Image.NEAREST)
    img.save(path)
    print("preview ->", path)

def main():
    words = load_strip()
    os.makedirs(OUTDIR, exist_ok=True)
    os.makedirs(SRCDIR, exist_ok=True)
    named = {"term.icn": 4, "clock.icn": 9}   # window box / round dial
    for n in range(NICONS):
        rows = icon_rows(words, n)
        for d in (OUTDIR, SRCDIR):
            write_icn(os.path.join(d, "icon%d.icn" % n), rows)
    for name, n in named.items():
        rows = icon_rows(words, n)
        for d in (OUTDIR, SRCDIR):
            write_icn(os.path.join(d, name), rows)
    print("wrote icon0..icon%d.icn + %s to %s and %s"
          % (NICONS - 1, ",".join(named), OUTDIR, SRCDIR))
    if len(sys.argv) > 1:
        preview(words, sys.argv[1])

if __name__ == "__main__":
    main()
