#!/usr/bin/env python3
"""verify.py -- check a packed disk image against a reference image.

Because the image is packed from freshly-built binaries (build/root), file
CONTENT is NOT expected to be byte-identical to the reference -- so content is
reported informationally.  What must match is the STRUCTURE the OS depends on:
per-partition geometry/free counts, the set of paths, permissions/owners, the
/dev nodes, and the hardlink groups.

The reference image is whatever original C900 disk you are comparing against;
there is no default, since the build no longer depends on one.

Usage: python verify.py [built-image] REFERENCE-IMAGE
"""

import os
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_ROOT = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, SCRIPT_DIR)                 # tools/disk.py
import disk
from disk import CoherentFS, ROOTINO, SB_ISIZE, SB_FSIZE, SB_TFREE, SB_TINODE

if len(sys.argv) < 2:
    sys.exit("usage: verify.py [built-image] REFERENCE-IMAGE")
if len(sys.argv) > 2:
    BUILT, MASTER = sys.argv[1], sys.argv[2]
else:
    BUILT = os.path.join(SRC_ROOT, "build", "dist", "hdd.bin")
    MASTER = sys.argv[1]

A = bytearray(open(MASTER, "rb").read())
B = bytearray(open(BUILT, "rb").read())


def collect(img, start):
    fs = CoherentFS(img, start)
    out = {}

    def rec(ino, path, seen):
        for cino, name in sorted(fs.read_dir(fs.stat(ino)), key=lambda e: e[1]):
            c = fs.stat(cino)
            if c is None:
                continue
            p = path + "/" + name
            t = c.mode & 0o170000
            if t in (0o020000, 0o060000):
                val = ("dev", c.addrs[0])
            elif t == 0o100000:
                val = ("file", len(fs.read_file(c)), hash(fs.read_file(c)))
            else:
                val = ("dir",)
            out[p] = dict(t=t, perm=c.mode & 0o7777, uid=c.uid, gid=c.gid,
                          nl=c.nlinks, val=val, ino=cino)
            if c.is_dir() and cino not in seen:
                seen.add(cino)
                rec(cino, p, seen)

    r = fs.stat(ROOTINO)
    out["/"] = dict(t=r.mode & 0o170000, perm=r.mode & 0o7777, uid=r.uid,
                    gid=r.gid, nl=r.nlinks, val=("dir",), ino=ROOTINO)
    rec(ROOTINO, "", {ROOTINO})
    return out


def orphans(img, start):
    fs = CoherentFS(img, start)
    slots = (struct.unpack_from("<H", fs._read_sb(), SB_ISIZE)[0] - 2) * 8
    seen = {ROOTINO}

    def rec(ino):
        for cino, _ in fs.read_dir(fs.stat(ino)):
            if cino not in seen:
                seen.add(cino)
                c = fs.stat(cino)
                if c and c.is_dir():
                    rec(cino)
    rec(ROOTINO)
    return [i for i in range(2, slots + 1)
            if fs.stat(i) is not None and i not in seen]


problems = 0
orph = orphans(A, 0)
print("master orphan inodes (not reproduced):", orph or "none")

# --- partition-0 objects (needed up front for the inode-count expectation) --
ma, mb = collect(A, 0), collect(B, 0)
only_a = sorted(set(ma) - set(mb))
only_b = sorted(set(mb) - set(ma))
# distinct inodes present on one side only: intended build/root additions raise
# the built used-inode count; master-only objects (and orphans) lower it.
extra_inos = len({mb[p]["ino"] for p in only_b})
missing_inos = len({ma[p]["ino"] for p in only_a})

# --- geometry / free counts -------------------------------------------------
for i, (start, _) in enumerate(disk.WD_PARTITIONS):
    fa, fb = CoherentFS(A, start), CoherentFS(B, start)
    sa, sb = fa._read_sb(), fb._read_sb()
    for nm, off, is32 in [("isize", SB_ISIZE, 0), ("fsize", SB_FSIZE, 1),
                          ("tfree", SB_TFREE, 1), ("tinode", SB_TINODE, 0)]:
        va = fa.read32(sa, off) if is32 else struct.unpack_from("<H", sa, off)[0]
        vb = fb.read32(sb, off) if is32 else struct.unpack_from("<H", sb, off)[0]
        if nm == "tinode" and start == 0:
            # free inodes shift by exactly (orphans + missing - extra) objects
            exp = va + len(orph) + missing_inos - extra_inos
            ok = vb == exp
            tag = "" if vb == va else (
                "  (expected %d: %d extra file(s) in build/root, OK)" % (exp, extra_inos)
                if ok else "  <<< MISMATCH")
        elif nm == "tfree" and start == 0:
            # free blocks legitimately differ: rebuilt binaries aren't master-sized
            ok = True
            tag = "" if va == vb else "  (info: rebuilt sizes differ)"
        else:
            ok = va == vb
            tag = "" if ok else "  <<< MISMATCH"
        if not ok:
            problems += 1
        print("part %d %-7s master=%-7d built=%-7d%s" % (i, nm, va, vb, tag))

# --- partition-0 objects ----------------------------------------------------
for p in only_a:
    print("MISSING (in master, not built):", p, ma[p]["val"][0])
    problems += 1
for p in only_b:
    print("EXTRA (built, not in master):", p, mb[p]["val"][0])

# metadata must match for every shared path; content is informational
meta_bad = content_same = content_diff = 0
for p in sorted(set(ma) & set(mb)):
    a, b = ma[p], mb[p]
    if (a["t"], a["perm"], a["uid"], a["gid"]) != (b["t"], b["perm"], b["uid"], b["gid"]):
        print("META DIFF %s\n   master t=%o perm=%o %d/%d\n   built  t=%o perm=%o %d/%d"
              % (p, a["t"], a["perm"], a["uid"], a["gid"],
                 b["t"], b["perm"], b["uid"], b["gid"]))
        meta_bad += 1
        problems += 1
    if a["val"][0] == "file":
        if a["val"] == b["val"]:
            content_same += 1
        else:
            content_diff += 1
    elif a["val"][0] == "dev" and a["val"] != b["val"]:
        print("DEV DIFF %s master=%d built=%d" % (p, a["val"][1], b["val"][1]))
        problems += 1


def linkgroups(m):
    g = {}
    for p, v in m.items():
        if v["t"] == 0o100000:
            g.setdefault(v["ino"], set()).add(p)
    return sorted(tuple(sorted(s)) for s in g.values() if len(s) > 1)


if linkgroups(ma) != linkgroups(mb):
    print("HARDLINK GROUPS DIFFER")
    print("  master:", linkgroups(ma))
    print("  built :", linkgroups(mb))
    problems += 1
else:
    print("hardlink groups: %d, identical" % len(linkgroups(ma)))

print("content: %d files byte-identical to master, %d rebuilt/differ (expected)"
      % (content_same, content_diff))
print("metadata mismatches: %d" % meta_bad)
print("\nRESULT:", "STRUCTURE OK" if problems == 0 else "%d PROBLEM(S)" % problems)
sys.exit(1 if problems else 0)
