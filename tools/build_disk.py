#!/usr/bin/env python3
"""build_disk.py -- pack the build/root staging tree into a Coherent (C900)
hard-disk image (build/dist/hdd.bin).

This is the OS image-packing step of the build.  The file *contents* come from
build/root (what `make` produces); ownership, permissions and the /dev device
nodes -- which a Windows staging tree cannot carry -- are taken from the
manifests in src/image/.  See CLAUDE.md: "load-order/perms are applied when the
image is packed".

The result is a four-partition C900 disk (no MBR; partitions at the fixed WD-
driver offsets) formatted with the master's exact per-partition geometry
(inode-block count `isize` and filesystem size `fsize`): partition 0 (hd0, /)
populated from build/root, partitions 1-3 (/u /v /tmp) empty filesystems.

The filesystem primitives (mkfs allocator, block mapping, directory writer) come
from tools/disk.py alongside this script.

The manifests are CHECKED-IN, hand-maintained inputs.  They live in src/image/
with descriptive names:
  hdd_manifest.txt / hdd_devices.txt        (hard disk)
  floppy_manifest.txt / floppy_devices.txt  (floppy)

The pack formats the partitions, populates partition 0 from --root (perms +
hardlinks from --perms), injects the /dev nodes from --devices, and writes
--out.  It errors if any file in --root has no --perms entry -- the manifest
must stay authoritative.  A new file therefore has to be added to the manifest
as well as to build/root.

Usage:
  python build_disk.py [--root D] [--perms P] [--devices P] [--floppy] [--out P]
"""

import argparse
import os
import struct
import sys
import time

# The shared Coherent filesystem implementation, tools/disk.py in this repo.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_ROOT = os.path.dirname(SCRIPT_DIR)                       # .../Source
sys.path.insert(0, SCRIPT_DIR)
import disk
from disk import (CoherentFS, Inode, FSError, BLOCK, DIRENT_SIZE,
                  ROOTINO, S_IFREG, S_IFDIR, SB_ISIZE, SB_FSIZE, SB_NFREE,
                  SB_NINODE)

# C900 partition geometry (fixed by the WD driver) and each partition's own
# root-directory permissions, both taken from the shipped 20 MB MiniScribe disk.
#   start : first block of the partition (== disk.WD_PARTITIONS[i][0])
#   isize : first data block == 2 + inode blocks   (inode slots = (isize-2)*8)
#   fsize : total blocks the filesystem spans (<= the partition region)
PARTITIONS = [
    dict(start=0,     isize=187, fsize=10336, rmode=0o040755, ruid=0, rgid=1),
    dict(start=10336, isize=177, fsize=10336, rmode=0o040755, ruid=3, rgid=1),
    dict(start=20672, isize=177, fsize=10336, rmode=0o040755, ruid=3, rgid=1),
    dict(start=31008, isize=127, fsize=7000,  rmode=0o040777, ruid=3, rgid=1),
]
IMAGE_BYTES = 41616 * BLOCK          # HD image size: 41616 blocks (21307392 B)

# Single-partition C900 floppy (Coherent filesystem at block 0); geometry taken
# from the reference boot floppy disk1_hr.bin.  Selected with --floppy.
FLOPPY_PARTITIONS = [
    dict(start=0, isize=25, fsize=2300, rmode=0o040755, ruid=0, rgid=1),
]
FLOPPY_BYTES = 1184656               # == disk1_hr.bin

DEF_ROOT = os.path.join(SRC_ROOT, "build", "root")
# Checked-in manifests all live in src/image/ with descriptive names.
DEF_PERMS = os.path.join(SRC_ROOT, "src", "image", "hdd_manifest.txt")
DEF_DEVICES = os.path.join(SRC_ROOT, "src", "image", "hdd_devices.txt")
DEF_OUT = os.path.join(SRC_ROOT, "build", "dist", "hdd.bin")

# --------------------------------------------------------------------------
# mkfs + low-level writers
# --------------------------------------------------------------------------

def mkfs(image, start, isize, fsize):
    """Format a fresh Coherent/V7 filesystem: boot block (0), superblock (1),
    inode blocks (2..isize-1), inode 1 = reserved bad-block file, inode 2 =
    empty root dir, then a canonical free list rebuilt from the inode table."""
    fs = CoherentFS(image, start)
    for b in range(start, start + isize):
        image[b * BLOCK:(b + 1) * BLOCK] = bytes(BLOCK)

    sb = bytearray(BLOCK)
    struct.pack_into("<H", sb, SB_ISIZE, isize)
    fs.write32(sb, SB_FSIZE, fsize)
    struct.pack_into("<H", sb, SB_NFREE, 0)      # empty free list (rebuilt below)
    struct.pack_into("<H", sb, SB_NINODE, 0)
    fs._write_sb(sb)

    now = int(time.time())
    bad = Inode(fs, 1)
    bad.mode = S_IFREG                            # reserved bad-block inode
    bad.atime = bad.mtime = bad.ctime = now
    fs.write_inode(bad)

    root = Inode(fs, ROOTINO)
    root.mode = S_IFDIR | 0o755
    root.atime = root.mtime = root.ctime = now
    # A filesystem root carries an extra "mount" link beyond its own "." + ".."
    # (the reference by which it is mounted into a parent tree).  `dcheck`
    # pre-seeds the root's expected link count by 1 (entries[ROOTIN-1]++), and
    # every reference image -- floppy and each HD partition, empty ones included
    # -- has root nlink == 1 + (dir refs to root).  Seed 3 (== 1 mount + "." +
    # "..") so an empty root is 3 and create_dir bumps it to 3 + nsubdirs;
    # seeding 2 leaves every root one short ("Ino 2 Entries N Link N-1").
    root.nlinks = 3
    root.size = 2 * DIRENT_SIZE
    root.addrs[0] = isize
    fs.write_inode(root)
    blk = bytearray(BLOCK)
    blk[0:DIRENT_SIZE] = CoherentFS._encode_dirent(ROOTINO, ".")
    blk[DIRENT_SIZE:2 * DIRENT_SIZE] = CoherentFS._encode_dirent(ROOTINO, "..")
    fs.write_block(start + isize, bytes(blk))

    fs.rebuild_free_list()
    return fs


def set_meta(inode, mode_low, uid, gid):
    inode.mode = (inode.mode & 0o170000) | (mode_low & 0o7777)
    inode.uid = uid
    inode.gid = gid
    inode.owner.write_inode(inode)


def split_parent(fs, path):
    dpath, _, name = path.rpartition("/")
    parent = fs.lookup(dpath if dpath else "/")
    if parent is None:
        raise FSError("parent of %s does not exist" % path)
    return parent, name


def image_path(root_dir, host):
    rel = os.path.relpath(host, root_dir).replace(os.sep, "/")
    return "/" if rel == "." else "/" + rel


# --------------------------------------------------------------------------
# build: build/root + manifests -> fresh image
# --------------------------------------------------------------------------

def read_rows(path):
    rows = []
    with open(path, "r") as f:
        for line in f:
            line = line.rstrip("\n")
            if line and not line.startswith("#"):
                rows.append(line.split("\t"))
    return rows


def build(root_dir, perms_path, devices_path, out_path,
          parts=PARTITIONS, image_bytes=IMAGE_BYTES):
    if not os.path.isdir(root_dir):
        raise FSError("staging tree not found: %s (run `make` first)" % root_dir)
    perms = read_rows(perms_path)
    devices = read_rows(devices_path)

    meta = {}            # image path -> (type, mode, uid, gid)  for 'd' / 'f'
    links = []           # (path, target)  hardlinks: path is another name for target
    for row in perms:
        if row[0] == "l":
            links.append((row[1], row[2]))
        else:
            t, path, mode_s, uid_s, gid_s = row[:5]
            meta[path] = (t, int(mode_s, 8), int(uid_s), int(gid_s))
    link_paths = {p for p, _ in links}

    # The manifest is authoritative for ownership/permissions: every object in
    # build/root must have an entry (a 'd'/'f' line, or an 'l' hardlink).  A file
    # present in the staging tree but absent from the manifest is a hard error --
    # add a line for it to src/image/hdd_manifest.txt.
    declared = set(meta) | link_paths
    undeclared = sorted(
        image_path(root_dir, os.path.join(dp, nm))
        for dp, dns, fns in os.walk(root_dir) for nm in dns + fns
        if image_path(root_dir, os.path.join(dp, nm)) not in declared)
    if undeclared:
        raise FSError("%d path(s) in %s have no entry in %s:\n  %s"
                      % (len(undeclared), root_dir, perms_path,
                         "\n  ".join(undeclared)))

    image = bytearray(image_bytes)
    fss = []
    for p in parts:
        fs = mkfs(image, p["start"], p["isize"], p["fsize"])
        set_meta(fs.stat(ROOTINO), p["rmode"], p["ruid"], p["rgid"])
        fss.append(fs)
    fs0 = fss[0]
    if "/" in meta:
        _, mode, uid, gid = meta["/"]
        set_meta(fs0.stat(ROOTINO), mode, uid, gid)

    # -- Step A: every directory (manifest dirs + build/root dirs), path order.
    # create_dir maintains each directory's link count (2 + subdirs) as it goes.
    dirs = {p for p, m in meta.items() if m[0] == "d" and p != "/"}
    for dp, _, _ in os.walk(root_dir):
        ip = image_path(root_dir, dp)
        if ip != "/":
            dirs.add(ip)
    for path in sorted(dirs):
        parent, name = split_parent(fs0, path)
        node = fs0.lookup(path) or fs0.create_dir(parent, name)
        set_meta(node, *meta[path][1:])

    # -- Step B: regular files from build/root (link paths are made in Step C) --
    files = [os.path.join(dp, fn)
             for dp, _, fns in os.walk(root_dir) for fn in fns]
    for host in sorted(files, key=lambda h: image_path(root_dir, h)):
        ipath = image_path(root_dir, host)
        if ipath in link_paths:
            continue
        parent, name = split_parent(fs0, ipath)
        with open(host, "rb") as f:
            data = f.read()
        node = fs0.create_file(parent, name, data, meta[ipath][1])   # nlinks = 1
        set_meta(node, *meta[ipath][1:])

    # -- Step C: hardlinks -- a second directory entry for the target's inode --
    for path, target in links:
        tnode = fs0.lookup(target)
        if tnode is None:
            raise FSError("hardlink %s -> missing target %s" % (path, target))
        parent, name = split_parent(fs0, path)
        fs0.add_dir_entry(parent, name, tnode.ino)
        tnode.nlinks += 1
        fs0.write_inode(tnode)

    # -- Step D: /dev nodes from the device manifest (device inodes are nlink 1).
    # The on-disk dev word is (minor << 16) | major.
    now = int(time.time())
    for t, path, mode_s, uid_s, gid_s, maj, minr in devices:
        parent, name = split_parent(fs0, path)
        ino = fs0.alloc_inode()
        node = Inode(fs0, ino)
        node.mode = (0o020000 if t == "c" else 0o060000) | (int(mode_s, 8) & 0o7777)
        node.nlinks = 1
        node.uid, node.gid = int(uid_s), int(gid_s)
        node.addrs[0] = (int(minr) << 16) | int(maj)
        node.atime = node.mtime = node.ctime = now
        fs0.write_inode(node)
        fs0.add_dir_entry(parent, name, ino)

    for fs in fss:
        fs.rebuild_free_list()

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(image)
    print("build: packed %s -> %s (%d bytes)"
          % (root_dir, out_path, len(image)))


# --------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=DEF_ROOT, help="build/root staging tree")
    ap.add_argument("--perms", default=DEF_PERMS,
                    help="permissions/owners manifest (default src/image/hdd_manifest.txt)")
    ap.add_argument("--devices", default=DEF_DEVICES,
                    help="device manifest (default src/image/hdd_devices.txt)")
    ap.add_argument("--floppy", action="store_true",
                    help="single-partition C900 floppy layout instead of the 4-partition HD")
    ap.add_argument("--out", default=DEF_OUT, help="output image path")
    args = ap.parse_args(argv)

    parts = FLOPPY_PARTITIONS if args.floppy else PARTITIONS
    image_bytes = FLOPPY_BYTES if args.floppy else IMAGE_BYTES
    build(args.root, args.perms, args.devices, args.out, parts, image_bytes)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FSError, OSError) as e:
        print("error: %s" % e, file=sys.stderr)
        sys.exit(1)
