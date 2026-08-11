#!/usr/bin/env python3
"""Turn an ld -n image into a Coherent shared library (.sl).

Usage: mkslib.py [--slot N] IMAGE

Patches the l.out header of IMAGE in place:
  - validates magic 0407 / machine M_Z8001 / LF_SHR set / LF_SEP clear,
  - checks the shared half (SHRI+SHRD) and the private half
    (PRVI+PRVD+BSSI+BSSD) each fit one 64 K hardware segment,
  - sets l_entry to the slot's text base (SLS0+slot)<<24 -- the kernel
    derives the library slot from it, and ld derives the client's
    LF_SLREF0/1 marking from it (the linker's default entry is
    `userbase`, which would be wrong for a library),
  - ORs LF_SLIB into l_flag,
  - demotes the layout symbols (end_/edata_/etext_/__end_) from global
    in the symbol table, so a client referencing `end' binds its OWN
    linker-computed value, never the library's,
and prints a segment-usage report (the growth tripwire).

Byte order is canonical PDP-11: little-endian 16-bit words, longs with
the most-significant word first.  Header layout (48 bytes): 4 shorts
(l_magic, l_flag, l_machine, l_tbase), 9 longs l_ssize[], long l_entry.
"""

import argparse
import struct
import sys

L_MAGIC = 0o407
M_Z8001 = 4
LF_SHR, LF_SEP, LF_NRB, LF_KER, LF_32 = 0o1, 0o2, 0o4, 0o10, 0o20
LF_SLREF, LF_SLIB = 0o40, 0o100
SLS0 = 0x34                     # first library text segment (kernel machine.h)
NSLIB = 2
SEGSIZE = 0x10000
SEGNAMES = ["SHRI", "PRVI", "BSSI", "SHRD", "PRVD", "BSSD", "DEBUG", "SYM", "REL"]


def get_long(b, off):
    w0, w1 = struct.unpack_from("<2H", b, off)
    return (w0 << 16) | w1


def put_long(b, off, val):
    struct.pack_into("<2H", b, off, (val >> 16) & 0xFFFF, val & 0xFFFF)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--slot", type=int, default=0, help="library slot (0..%d)" % (NSLIB - 1))
    ap.add_argument("image")
    args = ap.parse_args()
    if not 0 <= args.slot < NSLIB:
        sys.exit("mkslib: slot must be 0..%d" % (NSLIB - 1))

    with open(args.image, "rb") as f:
        data = bytearray(f.read())
    if len(data) < 48:
        sys.exit("mkslib: %s: too short for an l.out header" % args.image)

    magic, flag, machine, tbase = struct.unpack_from("<4h", data, 0)
    ssize = [get_long(data, 8 + 4 * i) for i in range(9)]
    shri, prvi, bssi, shrd, prvd, bssd = ssize[:6]

    if magic != L_MAGIC:
        sys.exit("mkslib: %s: bad magic %o (want %o)" % (args.image, magic & 0xFFFF, L_MAGIC))
    if machine != M_Z8001:
        sys.exit("mkslib: %s: machine %d is not Z8001" % (args.image, machine))
    if not flag & LF_SHR or flag & LF_SEP:
        sys.exit("mkslib: %s: must be linked -n (LF_SHR) and not -i (flag %o)"
                 % (args.image, flag & 0xFFFF))
    if flag & LF_KER:
        sys.exit("mkslib: %s: LF_KER image cannot be a shared library" % args.image)
    shared = shri + shrd
    private = prvi + prvd + bssi + bssd
    if shared == 0:
        sys.exit("mkslib: %s: empty shared half" % args.image)
    if shared > SEGSIZE:
        sys.exit("mkslib: %s: shared half %d exceeds 64 K" % (args.image, shared))
    if private > SEGSIZE:
        sys.exit("mkslib: %s: private half %d exceeds 64 K" % (args.image, private))

    entry = (SLS0 + args.slot) << 24
    struct.pack_into("<H", data, 2, (flag | LF_SLIB) & 0xFFFF)
    put_long(data, 8 + 4 * 9, entry)

    # Demote the per-binary layout symbols so clients never import them:
    # ld's read_absolute_symbols() skips entries without L_GLOBAL (020).
    L_GLOBAL = 0o20
    hide = {b"end_", b"edata_", b"etext_", b"__end_"}
    symoff = 48 + shri + prvi + shrd + prvd + ssize[6]
    nsym = ssize[7] // 22
    hidden = []
    for i in range(nsym):
        off = symoff + i * 22
        rec = data[off:off + 22]
        if len(rec) < 22:
            break
        name = bytes(rec[:16]).split(b"\0")[0]
        (stype,) = struct.unpack_from("<h", data, off + 16)
        if name in hide and stype & L_GLOBAL:
            struct.pack_into("<h", data, off + 16, stype & ~L_GLOBAL)
            hidden.append(name.decode())

    with open(args.image, "r+b") as f:
        f.write(data)

    print("mkslib: %s: slot %d, text @ segment 0x%02X, data @ segment %d"
          % (args.image, args.slot, SLS0 + args.slot, 1 + args.slot))
    if hidden:
        print("  hidden from clients: %s" % " ".join(hidden))
    for i, n in enumerate(SEGNAMES):
        if ssize[i]:
            print("  %-5s %6d" % (n, ssize[i]))
    print("  shared half  %6d / %d bytes (%2d%% of segment 0x%02X)"
          % (shared, SEGSIZE, 100 * shared // SEGSIZE, SLS0 + args.slot))
    print("  private half %6d / %d bytes (%2d%% of segment %d, ~%d KB per process)"
          % (private, SEGSIZE, 100 * private // SEGSIZE, 1 + args.slot,
             (private + 1023) // 1024))


if __name__ == "__main__":
    main()
