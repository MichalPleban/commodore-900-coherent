#!/usr/bin/env python3
"""disk.py -- the Coherent (C900) filesystem, and a CLI to inspect/edit images.

This is the shared filesystem implementation the image build depends on:
build_disk.py, verify.py and damage_fs.py all `import disk` from this
directory.  The on-disk inode/dirent codec here must match the kernel structs
in src/include/sys/{ino,filsys,dir}.h -- when those change, change this too.

A copy of this module lives in the emulator repo as ../Emulator/tools/disk.py
(the image-inspection tool documented in CLAUDE.md); the code is the same, only
this header differs.  The two are duplicates, not a package: a fix to the
on-disk format has to be applied to both.

Standalone Python implementation of the write side of the Coherent
filesystem: superblock free-list allocator (V7 balloc/bfree), inode allocator
(ialloc/ifree), direct + single/double/triple indirect block mapping, and
16-byte directory entries. The on-disk format is the Coherent/PDP-11 layout:
middle-endian 32-bit fields (i_size, daddr_t, times), 32-bit indirect-block
entries, and root inode 2 — used on both the hard disk and floppies.

The C900 hard disk has no MBR; its partitions live at fixed block offsets
hardcoded in the WD driver (see WD_PARTITIONS below, from wd.c wdbtab[]). hd0
is the root filesystem; /etc/rc on it is parsed so mounts like /dev/hd1 -> /u
resolve to the right partition. Because that /etc/rc parsing is fragile, pass
--partition N to operate on one partition directly and skip it entirely.

Usage:
  python disk.py IMAGE --write [options] FILE [FILE ...]
  python disk.py IMAGE --read  [options] FILE [FILE ...]
  python disk.py IMAGE --fsck  [--partition N]
  python disk.py IMAGE --ls PATH

One of --write, --read, --fsck, or --ls is required.

--fsck checks and repairs one partition (chosen by --partition, default 0): it
rebuilds the free-block chain from the inode table — the offline `icheck -s`
the guest OS doesn't ship — which fixes the stale free list left when the
emulator is stopped without a `sync`. It also reports cross-linked and
out-of-range blocks (real file-content damage a free-list rebuild cannot undo).

--write copies host -> image (the original behaviour). Each FILE is a host
path, or SRC=DST to choose the in-image destination. A host directory is
copied recursively. Plain paths land under --dest (default /):
      ... --write kernel=/coherent
      ... --write --dest /usr/bin --mode 755 ls cat
      ... --write --mkdirs --dest /new/dir a.txt
      ... --write --dest /usr sometree          # whole tree -> /usr/sometree

--read copies image -> host. Each FILE is an in-image path, or SRC=DST to
choose the host destination. An in-image directory is extracted recursively.
Plain paths land under --dest (default the current directory):
      ... --read /etc/motd                      # -> ./motd
      ... --read /bin=out/bin                    # extract /bin tree -> out/bin

On --write the image is modified IN PLACE unless --out is given; keep a
backup. Existing files are overwritten in place (mode preserved unless --mode
is given). --read never modifies the image.
"""

import argparse
import os
import struct
import sys
import time

BLOCK = 512
INODE_SIZE = 64
INODES_PER_BLOCK = BLOCK // INODE_SIZE
DIRENT_SIZE = 16
MAX_NAME = 14
NDIRECT = 10
NICFREE = 64
NICINODE = 100

# struct filsys field offsets
SB_ISIZE = 0    # unsigned short
SB_FSIZE = 2    # daddr_t (middle-endian 32-bit)
SB_NFREE = 6    # short
SB_FREE = 8     # daddr_t[64]
SB_NINODE = 264  # short
SB_INODE = 266  # ino_t[100]
SB_FLOCK = 466  # char
SB_ILOCK = 467  # char
SB_FMOD = 468   # char
SB_TIME = 470   # time_t (middle-endian 32-bit)
SB_TFREE = 474  # daddr_t
SB_TINODE = 478  # ino_t (short)

# free-list chain block (struct fblk)
FBLK_NFREE = 0
FBLK_FREE = 2

S_IFMT = 0o170000
S_IFREG = 0o100000
S_IFDIR = 0o040000

ROOTINO = 2      # Coherent root inode (inode 1 is the bad-block inode)

# Indirect-block entries are 32-bit, so an indirect block holds 512/4 = 128.
INDIRECT_COUNT = BLOCK // 4

# C900 hard-disk partition table, hardcoded in the WD driver
# (Source/src/kernel/z8001/drv/wd.c, wdbtab[]) — there is no MBR. The
# pseudo-drive minor number selects one of these fixed (start, size) block
# ranges. On the shipped 20 MB MiniScribe image hd0=/ hd1=/u hd2=/v hd3=/tmp.
WD_PARTITIONS = [
    (0,     10336),   # hd0
    (10336, 10336),   # hd1
    (20672, 10336),   # hd2
    (31008, 10336),   # hd3
]


class FSError(Exception):
    pass


def _s16(v):
    """Reinterpret a uint16 as a signed int16 (the kernel stores shorts)."""
    return v - 0x10000 if v > 0x7FFF else v


class Inode:
    __slots__ = ("ino", "mode", "nlinks", "uid", "gid", "size",
                 "atime", "mtime", "ctime", "addrs", "owner")

    def __init__(self, owner, ino):
        self.owner = owner
        self.ino = ino
        self.mode = 0
        self.nlinks = 0
        self.uid = 0
        self.gid = 0
        self.size = 0
        self.atime = 0
        self.mtime = 0
        self.ctime = 0
        self.addrs = [0] * 13

    def is_file(self):
        return self.mode & 0xF000 == S_IFREG

    def is_dir(self):
        return self.mode & 0xF000 == S_IFDIR


class FsckReport:
    """One partition's block accounting from a filesystem check."""

    def __init__(self, isize, fsize):
        self.isize = isize          # first data block (0..isize-1 = boot+super+inodes)
        self.fsize = fsize          # total blocks in the partition
        self.used_inodes = 0        # inodes with nonzero mode
        self.free_inodes = 0        # inode-table slots with mode 0
        self.used_blocks = 0        # distinct data+indirect blocks in use
        self.free_blocks = 0        # fsize-isize - used_blocks (what a rebuild chains)
        # block -> [inodes] for any block claimed by more than one inode
        # (real file damage: a later writer overwrote an earlier file's data)
        self.cross_linked = {}
        # inode -> [block pointers outside [isize, fsize)]
        self.out_of_range = {}

    def __str__(self):
        s = ("isize=%d fsize=%d inodes used/free=%d/%d blocks used/free=%d/%d"
             % (self.isize, self.fsize, self.used_inodes, self.free_inodes,
                self.used_blocks, self.free_blocks))
        if self.cross_linked:
            s += " CROSS-LINKED=%d" % len(self.cross_linked)
        if self.out_of_range:
            s += " OUT-OF-RANGE-INODES=%d" % len(self.out_of_range)
        return s

    def damaged(self):
        return bool(self.cross_linked or self.out_of_range)


class CoherentFS:
    """One Coherent filesystem (= one partition) inside a shared image
    bytearray. The root (hd0) handle additionally carries the /etc/rc
    mount table so absolute paths cross partitions transparently."""

    def __init__(self, image, part_off, root_ino=ROOTINO):
        self.image = image          # shared bytearray, mutated in place
        self.part_off = part_off    # partition offset in blocks
        self.root_ino = root_ino
        self.mounts = []            # [(path, CoherentFS)] on root handle only

    # -- field codecs (Coherent/PDP-11 middle-endian on-disk layout) ------

    def read32(self, buf, off):
        """32-bit field stored middle-endian: high 16-bit word first."""
        hi, lo = struct.unpack_from("<HH", buf, off)
        return (hi << 16) | lo

    def write32(self, buf, off, v):
        v &= 0xFFFFFFFF
        struct.pack_into("<HH", buf, off, (v >> 16) & 0xFFFF, v & 0xFFFF)

    def read_addr24(self, buf, off):
        """24-bit block address: high byte first, then a 16-bit LE word."""
        hi = buf[off]
        lo = struct.unpack_from("<H", buf, off + 1)[0]
        return (hi << 16) | lo

    def write_addr24(self, buf, off, v):
        v &= 0x00FFFFFF
        buf[off] = (v >> 16) & 0xFF
        struct.pack_into("<H", buf, off + 1, v & 0xFFFF)

    def indirect_count(self):
        return INDIRECT_COUNT

    def read_indirect_entry(self, blk, idx):
        return self.read32(blk, idx * 4)

    def write_indirect_entry(self, blk, idx, val):
        if idx < 0 or idx >= INDIRECT_COUNT:
            raise FSError("indirect entry idx %d out of range" % idx)
        self.write32(blk, idx * 4, val)

    # -- block I/O ------------------------------------------------------

    def read_block(self, bno):
        """Absolute block -> 512-byte bytes (zeroes past EOF)."""
        off = bno * BLOCK
        if off < 0 or off + BLOCK > len(self.image):
            return bytes(BLOCK)
        return bytes(self.image[off:off + BLOCK])

    def write_block(self, bno, data):
        if len(data) != BLOCK:
            raise FSError("write_block(%d): %d bytes, want %d" % (bno, len(data), BLOCK))
        off = bno * BLOCK
        if off < 0 or off + BLOCK > len(self.image):
            raise FSError("write_block(%d) out of range (image %d bytes)" % (bno, len(self.image)))
        self.image[off:off + BLOCK] = data

    # -- inode table ------------------------------------------------------

    def stat(self, ino):
        if ino < 1:
            return None
        idx = ino - 1
        blk_num = self.part_off + 2 + idx // INODES_PER_BLOCK
        off = (idx % INODES_PER_BLOCK) * INODE_SIZE
        raw = self.read_block(blk_num)
        mode = struct.unpack_from("<H", raw, off)[0]
        if mode == 0:
            return None
        inode = Inode(self, ino)
        inode.mode = mode
        inode.nlinks, inode.uid, inode.gid = struct.unpack_from("<HHH", raw, off + 2)
        inode.size = self.read32(raw, off + 8)
        inode.addrs = [self.read_addr24(raw, off + 12 + j * 3) for j in range(13)]
        # struct dinode: the address union is char di_addb[40] (12..51), of
        # which only NADDR*3 = 39 bytes are used -- byte 51 is padding and the
        # three time_t fields start at 52 (2+2+2+2+4+40+4+4+4 == 64).
        inode.atime = self.read32(raw, off + 52)
        inode.mtime = self.read32(raw, off + 56)
        inode.ctime = self.read32(raw, off + 60)
        return inode

    def write_inode(self, inode):
        idx = inode.ino - 1
        blk_num = self.part_off + 2 + idx // INODES_PER_BLOCK
        off = (idx % INODES_PER_BLOCK) * INODE_SIZE
        out = bytearray(self.read_block(blk_num))
        struct.pack_into("<HHHH", out, off,
                         inode.mode & 0xFFFF, inode.nlinks & 0xFFFF,
                         inode.uid & 0xFFFF, inode.gid & 0xFFFF)
        self.write32(out, off + 8, inode.size)
        for j in range(13):
            self.write_addr24(out, off + 12 + j * 3, inode.addrs[j])
        self.write32(out, off + 52, inode.atime)
        self.write32(out, off + 56, inode.mtime)
        self.write32(out, off + 60, inode.ctime)
        # byte 51 (di_addb padding) left as-is by rebuilding from the original block
        self.write_block(blk_num, bytes(out))

    # -- file content (read side, needed for dirs) -----------------------

    HOLE = -1

    def file_blocks(self, inode):
        """Logical->absolute block list, HOLE for sparse gaps."""
        need = (inode.size + BLOCK - 1) // BLOCK
        if need <= 0:
            return []
        blocks = []
        for b in inode.addrs[:10]:
            if len(blocks) >= need:
                return blocks
            blocks.append(self.part_off + b if b else self.HOLE)
        self._walk_indirect(inode.addrs[10], 1, need, blocks)
        self._walk_indirect(inode.addrs[11], 2, need, blocks)
        self._walk_indirect(inode.addrs[12], 3, need, blocks)
        return blocks

    def _span(self, depth):
        return self.indirect_count() ** depth

    def _walk_indirect(self, bno, depth, need, out):
        if len(out) >= need:
            return
        if bno == 0:
            for _ in range(self._span(depth)):
                if len(out) >= need:
                    return
                out.append(self.HOLE)
            return
        blk = self.read_block(self.part_off + bno)
        for i in range(self.indirect_count()):
            if len(out) >= need:
                return
            entry = self.read_indirect_entry(blk, i)
            if depth == 1:
                out.append(self.part_off + entry if entry else self.HOLE)
            else:
                self._walk_indirect(entry, depth - 1, need, out)

    def read_file(self, inode):
        out = bytearray()
        remaining = inode.size
        for b in self.file_blocks(inode):
            if remaining <= 0:
                break
            blk = bytes(BLOCK) if b == self.HOLE else self.read_block(b)
            take = min(BLOCK, remaining)
            out += blk[:take]
            remaining -= take
        return bytes(out)

    def read_dir(self, inode):
        """[(ino, name)] excluding '.' and '..'."""
        if not inode.is_dir():
            return []
        if inode.owner is not self:
            return inode.owner.read_dir(inode)
        body = self.read_file(inode)
        out = []
        for off in range(0, len(body) - DIRENT_SIZE + 1, DIRENT_SIZE):
            ino = struct.unpack_from("<H", body, off)[0]
            if ino == 0:
                continue
            name = body[off + 2:off + DIRENT_SIZE].split(b"\0", 1)[0].decode("latin-1")
            if name in (".", ".."):
                continue
            out.append((ino, name))
        return out

    # -- superblock + allocators ------------------------------------------

    def _read_sb(self):
        return bytearray(self.read_block(self.part_off + 1))

    def _write_sb(self, sb):
        self.write_block(self.part_off + 1, bytes(sb))

    def _touch_sb(self, sb):
        sb[SB_FMOD] = 1
        self.write32(sb, SB_TIME, int(time.time()))

    def alloc_block(self):
        """Pop one free block (partition-relative). V7 balloc."""
        sb = self._read_sb()
        nfree = _s16(struct.unpack_from("<H", sb, SB_NFREE)[0])
        if nfree <= 0:
            raise FSError("out of free blocks")
        nfree -= 1
        b = self.read32(sb, SB_FREE + nfree * 4)
        if b == 0:
            # enospc sentinel
            struct.pack_into("<H", sb, SB_NFREE, 0)
            self._write_sb(sb)
            raise FSError("out of free blocks")
        struct.pack_into("<H", sb, SB_NFREE, nfree)

        isize = struct.unpack_from("<H", sb, SB_ISIZE)[0]
        fsize = self.read32(sb, SB_FSIZE)
        if nfree == 0:
            # cache emptied: b is the chain pointer; refill from its fblk
            if b < isize or b >= fsize:
                raise FSError("bad free-list chain block %d" % b)
            fblk = self.read_block(self.part_off + b)
            chain_n = _s16(struct.unpack_from("<H", fblk, FBLK_NFREE)[0])
            if chain_n < 0 or chain_n > NICFREE:
                raise FSError("bad chain df_nfree %d" % chain_n)
            sb[SB_FREE:SB_FREE + NICFREE * 4] = fblk[FBLK_FREE:FBLK_FREE + NICFREE * 4]
            struct.pack_into("<H", sb, SB_NFREE, chain_n)

        if b < isize or b >= fsize:
            raise FSError("allocated block %d out of [%d, %d)" % (b, isize, fsize))

        tfree = self.read32(sb, SB_TFREE)
        if tfree > 0:
            self.write32(sb, SB_TFREE, tfree - 1)
        self._touch_sb(sb)
        self._write_sb(sb)
        return b

    def free_block(self, b):
        """Push a partition-relative block back onto the free list. V7 bfree."""
        if b == 0:
            raise FSError("refusing to free block 0")
        sb = self._read_sb()
        isize = struct.unpack_from("<H", sb, SB_ISIZE)[0]
        fsize = self.read32(sb, SB_FSIZE)
        if b < isize or b >= fsize:
            raise FSError("free_block(%d) out of [%d, %d)" % (b, isize, fsize))
        nfree = _s16(struct.unpack_from("<H", sb, SB_NFREE)[0])
        if nfree < 0 or nfree > NICFREE:
            raise FSError("corrupt s_nfree=%d" % nfree)
        if nfree == NICFREE:
            # Cache full: spill it into b, which becomes the next chain link
            # (its df_free[0] points on down the chain).  V7 bfree spills ONLY
            # at NICFREE -- never at nfree==0.  Spilling an *empty* cache would
            # write a 0 into the chain (the old cache's zeroed df_free[0]); that
            # 0 then gets counted/marked as a free block by icheck, tripping
            # "1 dup in free" (block 0 == boot block) and a tfree miscount.  The
            # chain instead terminates naturally: the deepest link points at a
            # never-spilled data block whose df_nfree reads 0.
            fblk = bytearray(BLOCK)
            struct.pack_into("<H", fblk, FBLK_NFREE, nfree)
            fblk[FBLK_FREE:FBLK_FREE + NICFREE * 4] = sb[SB_FREE:SB_FREE + NICFREE * 4]
            self.write_block(self.part_off + b, bytes(fblk))
            nfree = 0
        self.write32(sb, SB_FREE + nfree * 4, b)
        struct.pack_into("<H", sb, SB_NFREE, nfree + 1)
        self.write32(sb, SB_TFREE, self.read32(sb, SB_TFREE) + 1)
        self._touch_sb(sb)
        self._write_sb(sb)

    def alloc_inode(self):
        sb = self._read_sb()
        ninode = _s16(struct.unpack_from("<H", sb, SB_NINODE)[0])
        if ninode == 0:
            # refill cache by scanning the inode table for mode==0 slots
            isize = struct.unpack_from("<H", sb, SB_ISIZE)[0]
            ino, idx = 1, 0
            for blk in range(2, isize):
                if idx >= NICINODE:
                    break
                body = self.read_block(self.part_off + blk)
                for off in range(0, BLOCK, INODE_SIZE):
                    if idx >= NICINODE:
                        break
                    if struct.unpack_from("<H", body, off)[0] == 0:
                        struct.pack_into("<H", sb, SB_INODE + idx * 2, ino)
                        idx += 1
                    ino += 1
            ninode = idx
            struct.pack_into("<H", sb, SB_NINODE, ninode)
            if ninode == 0:
                struct.pack_into("<H", sb, SB_TINODE, 0)
                self._touch_sb(sb)
                self._write_sb(sb)
                raise FSError("out of free inodes")
        ninode -= 1
        ino = struct.unpack_from("<H", sb, SB_INODE + ninode * 2)[0]
        struct.pack_into("<H", sb, SB_NINODE, ninode)
        tinode = struct.unpack_from("<H", sb, SB_TINODE)[0]
        if tinode > 0:
            struct.pack_into("<H", sb, SB_TINODE, tinode - 1)
        self._touch_sb(sb)
        self._write_sb(sb)
        return ino

    def free_inode(self, ino):
        if ino < 1:
            raise FSError("free_inode(%d)" % ino)
        idx = ino - 1
        blk_num = self.part_off + 2 + idx // INODES_PER_BLOCK
        off = (idx % INODES_PER_BLOCK) * INODE_SIZE
        out = bytearray(self.read_block(blk_num))
        out[off:off + INODE_SIZE] = bytes(INODE_SIZE)
        self.write_block(blk_num, bytes(out))
        sb = self._read_sb()
        ninode = _s16(struct.unpack_from("<H", sb, SB_NINODE)[0])
        if ninode < NICINODE:
            struct.pack_into("<H", sb, SB_INODE + ninode * 2, ino)
            struct.pack_into("<H", sb, SB_NINODE, ninode + 1)
        tinode = struct.unpack_from("<H", sb, SB_TINODE)[0]
        struct.pack_into("<H", sb, SB_TINODE, tinode + 1)
        self._touch_sb(sb)
        self._write_sb(sb)

    # -- block mapping grow/shrink ----------------------------------------

    @staticmethod
    def _classify_index(n, idx):
        """logical block idx -> (depth, slot, coords)."""
        if idx < NDIRECT:
            return 0, idx, []
        r = idx - NDIRECT
        if r < n:
            return 1, None, [r]
        r -= n
        if r < n * n:
            return 2, None, [r // n, r % n]
        r -= n * n
        if r < n ** 3:
            return 3, None, [r // (n * n), (r % (n * n)) // n, r % n]
        raise FSError("file too large for triple indirect")

    def _zero_block(self, rel_blk):
        self.write_block(self.part_off + rel_blk, bytes(BLOCK))

    def _ensure_data_block(self, inode, idx):
        depth, slot, coords = self._classify_index(self.indirect_count(), idx)
        if depth == 0:
            if inode.addrs[slot] == 0:
                inode.addrs[slot] = self.alloc_block()
            return inode.addrs[slot]
        root_slot = NDIRECT + depth - 1
        if inode.addrs[root_slot] == 0:
            b = self.alloc_block()
            inode.addrs[root_slot] = b
            self._zero_block(b)
        return self._ensure_indirect_child(inode.addrs[root_slot], depth, coords)

    def _ensure_indirect_child(self, indir_blk, depth, coords):
        blk = bytearray(self.read_block(self.part_off + indir_blk))
        slot = coords[0]
        cur = self.read_indirect_entry(blk, slot)
        if depth == 1:
            if cur == 0:
                cur = self.alloc_block()
                self.write_indirect_entry(blk, slot, cur)
                self.write_block(self.part_off + indir_blk, bytes(blk))
            return cur
        if cur == 0:
            cur = self.alloc_block()
            self._zero_block(cur)
            self.write_indirect_entry(blk, slot, cur)
            self.write_block(self.part_off + indir_blk, bytes(blk))
        return self._ensure_indirect_child(cur, depth - 1, coords[1:])

    def _grow_to_n_blocks(self, inode, n_blocks):
        return [self._ensure_data_block(inode, i) for i in range(n_blocks)]

    def _shrink_to_n_blocks(self, inode, n_blocks):
        for i in range(n_blocks, NDIRECT):
            if inode.addrs[i]:
                self.free_block(inode.addrs[i])
                inode.addrs[i] = 0
        n = self.indirect_count()
        cuts = [max(0, n_blocks - NDIRECT),
                max(0, n_blocks - NDIRECT - n),
                max(0, n_blocks - NDIRECT - n - n * n)]
        caps = [n, n * n, n ** 3]
        for d in range(3):
            depth = d + 1
            root_slot = NDIRECT + d
            root = inode.addrs[root_slot]
            if root == 0:
                continue
            keep = min(cuts[d], caps[d])
            if self._shrink_indirect(root, depth, keep):
                self.free_block(root)
                inode.addrs[root_slot] = 0

    def _shrink_indirect(self, indir_blk, depth, keep):
        """Prune subtree to its first `keep` leaves. True if now empty."""
        work = bytearray(self.read_block(self.part_off + indir_blk))
        n = self.indirect_count()
        dirty = False
        all_zero = True
        for slot in range(n):
            entry = self.read_indirect_entry(work, slot)
            if entry == 0:
                continue
            if depth == 1:
                if slot < keep:
                    all_zero = False
                    continue
                self.free_block(entry)
                self.write_indirect_entry(work, slot, 0)
                dirty = True
            else:
                sub_span = n ** (depth - 1)
                base = slot * sub_span
                if base + sub_span <= keep:
                    all_zero = False
                    continue
                keep_sub = max(0, keep - base)
                if self._shrink_indirect(entry, depth - 1, keep_sub):
                    self.free_block(entry)
                    self.write_indirect_entry(work, slot, 0)
                    dirty = True
                else:
                    all_zero = False
        if dirty and not all_zero:
            self.write_block(self.part_off + indir_blk, bytes(work))
        return all_zero

    # -- file + directory writers ----------------------------------------

    def write_file(self, inode, data):
        """Replace inode's content with data (shrink/grow as needed)."""
        if not inode.is_file():
            raise FSError("inode %d is not a regular file" % inode.ino)
        owner = inode.owner
        n_new = (len(data) + BLOCK - 1) // BLOCK
        n_old = (inode.size + BLOCK - 1) // BLOCK
        if n_new < n_old:
            owner._shrink_to_n_blocks(inode, n_new)
        blocks = owner._grow_to_n_blocks(inode, n_new)
        for i, rel in enumerate(blocks):
            chunk = bytes(data[i * BLOCK:(i + 1) * BLOCK]).ljust(BLOCK, b"\0")
            owner.write_block(owner.part_off + rel, chunk)
        now = int(time.time())
        inode.size = len(data)
        inode.atime = now
        inode.mtime = now
        inode.ctime = now
        owner.write_inode(inode)

    @staticmethod
    def _encode_dirent(ino, name):
        return struct.pack("<H", ino) + name.encode("latin-1").ljust(MAX_NAME, b"\0")

    def add_dir_entry(self, parent, name, child_ino):
        validate_name(name)
        if not parent.is_dir():
            raise FSError("parent ino %d is not a directory" % parent.ino)
        owner = parent.owner
        body = owner.read_file(parent)
        reuse_off = -1
        for off in range(0, len(body) - DIRENT_SIZE + 1, DIRENT_SIZE):
            ino = struct.unpack_from("<H", body, off)[0]
            if ino == 0:
                if reuse_off < 0:
                    reuse_off = off
                continue
            if body[off + 2:off + DIRENT_SIZE].split(b"\0", 1)[0].decode("latin-1") == name:
                raise FSError("entry %r already exists" % name)

        entry = self._encode_dirent(child_ino, name)
        if reuse_off >= 0:
            new_off = reuse_off
        else:
            # append: dirs grow in 16-byte steps, size stays entries*16
            new_off = parent.size
            parent.size += DIRENT_SIZE
        blocks = owner._grow_to_n_blocks(parent, (parent.size + BLOCK - 1) // BLOCK)
        blk_idx, off_in_blk = divmod(new_off, BLOCK)
        out = bytearray(owner.read_block(owner.part_off + blocks[blk_idx]))
        out[off_in_blk:off_in_blk + DIRENT_SIZE] = entry
        owner.write_block(owner.part_off + blocks[blk_idx], bytes(out))
        now = int(time.time())
        parent.mtime = now
        parent.ctime = now
        owner.write_inode(parent)

    def create_file(self, parent, name, data, mode):
        """New regular file `name` with `data` under directory inode parent."""
        validate_name(name)
        owner = parent.owner
        ino = owner.alloc_inode()
        now = int(time.time())
        inode = Inode(owner, ino)
        inode.mode = (mode & 0o7777) | S_IFREG
        inode.nlinks = 1
        inode.atime = inode.mtime = inode.ctime = now
        try:
            owner.write_inode(inode)
            owner.write_file(inode, data)
            self.add_dir_entry(parent, name, ino)
        except FSError:
            owner._shrink_to_n_blocks(inode, 0)
            owner.free_inode(ino)
            raise
        return inode

    def create_dir(self, parent, name, mode=0o755):
        """New directory with . / .. pre-seeded; bumps parent nlink."""
        validate_name(name)
        owner = parent.owner
        ino = owner.alloc_inode()
        try:
            data_blk = owner.alloc_block()
        except FSError:
            owner.free_inode(ino)
            raise
        dir_block = bytearray(BLOCK)
        dir_block[0:DIRENT_SIZE] = self._encode_dirent(ino, ".")
        dir_block[DIRENT_SIZE:2 * DIRENT_SIZE] = self._encode_dirent(parent.ino, "..")
        now = int(time.time())
        inode = Inode(owner, ino)
        inode.mode = (mode & 0o7777) | S_IFDIR
        inode.nlinks = 2
        inode.size = 2 * DIRENT_SIZE
        inode.atime = inode.mtime = inode.ctime = now
        inode.addrs[0] = data_blk
        try:
            owner.write_block(owner.part_off + data_blk, bytes(dir_block))
            owner.write_inode(inode)
            self.add_dir_entry(parent, name, ino)
        except FSError:
            owner.free_block(data_blk)
            owner.free_inode(ino)
            raise
        parent.nlinks += 1
        parent.mtime = parent.ctime = now
        owner.write_inode(parent)
        return inode

    # -- path resolution (with mounts) ------------------------------------

    def root(self):
        return self.stat(self.root_ino)

    def _mount_at(self, path):
        for mp, sub in self.mounts:
            if mp == path:
                return sub
        return None

    def lookup(self, path):
        """Resolve absolute path to an Inode, crossing mount points.
        Returns None if any component is missing."""
        cur = self.root()
        if cur is None:
            return None
        cur_path = ""
        m = self._mount_at(cur_path)
        if m is not None:
            cur = m.root()
        for part in path.split("/"):
            if part in ("", "."):
                continue
            if cur is None or not cur.is_dir():
                return None
            owner = cur.owner
            nxt = None
            for ino, name in owner.read_dir(cur):
                if name == part:
                    nxt = owner.stat(ino)
                    break
            if nxt is None:
                return None
            cur = nxt
            cur_path += "/" + part
            m = self._mount_at(cur_path)
            if m is not None:
                cur = m.root()
        return cur

    # -- filesystem check + free-list rebuild (offline `icheck -s`) --------

    def walk_inode_blocks(self, inode, visit):
        """Call visit(rel, is_indirect) for every partition-relative block the
        inode references — data blocks AND the indirect pointer blocks (both
        are 'in use' for free-list purposes). Bounded by i_size exactly like
        file_blocks, so stale indirect entries past EOF are not misread as
        allocations; holes (zero pointers) still consume logical positions."""
        need = (inode.size + BLOCK - 1) // BLOCK
        if need <= 0:
            return
        got = [0]
        for b in inode.addrs[:NDIRECT]:
            if got[0] >= need:
                return
            if b != 0:
                visit(b, False)
            got[0] += 1
        self._walk_indirect_blocks(inode.addrs[10], 1, need, got, visit)
        self._walk_indirect_blocks(inode.addrs[11], 2, need, got, visit)
        self._walk_indirect_blocks(inode.addrs[12], 3, need, got, visit)

    def _walk_indirect_blocks(self, bno, depth, need, got, visit):
        if got[0] >= need:
            return
        if bno == 0:
            got[0] = min(need, got[0] + self._span(depth))
            return
        visit(bno, True)
        # a wildly out-of-range pointer block must not be dereferenced; the
        # caller's range check has already recorded it
        if (self.part_off + bno + 1) * BLOCK > len(self.image):
            got[0] = min(need, got[0] + self._span(depth))
            return
        blk = self.read_block(self.part_off + bno)
        i = 0
        while i < self.indirect_count() and got[0] < need:
            entry = self.read_indirect_entry(blk, i)
            if depth == 1:
                if entry != 0:
                    visit(entry, False)
                got[0] += 1
            else:
                self._walk_indirect_blocks(entry, depth - 1, need, got, visit)
            i += 1

    def check_blocks(self):
        """Scan this partition's inode table and return an FsckReport.
        Read-only: detects cross-linked and out-of-range blocks."""
        sb = self._read_sb()
        isize = struct.unpack_from("<H", sb, SB_ISIZE)[0]
        fsize = self.read32(sb, SB_FSIZE)
        if isize < 2 or fsize <= isize:
            raise FSError("fsck: implausible superblock isize=%d fsize=%d" % (isize, fsize))
        rep = FsckReport(isize, fsize)
        owner = {}                       # block -> first inode that claimed it
        nino = (isize - 2) * INODES_PER_BLOCK
        for ino in range(1, nino + 1):
            inode = self.stat(ino)
            if inode is None:
                rep.free_inodes += 1
                continue
            rep.used_inodes += 1
            # only files and directories own data blocks; a device inode keeps
            # a dev number in addrs[0] and must not be walked
            if not inode.is_file() and not inode.is_dir():
                continue

            def visit(rel, indirect, _ino=ino):
                if rel < isize or rel >= fsize:
                    rep.out_of_range.setdefault(_ino, []).append(rel)
                    return
                if rel in owner:
                    if rel not in rep.cross_linked:
                        rep.cross_linked[rel] = [owner[rel]]
                    rep.cross_linked[rel].append(_ino)
                    return
                owner[rel] = _ino
                rep.used_blocks += 1

            self.walk_inode_blocks(inode, visit)
        rep.free_blocks = (fsize - isize) - rep.used_blocks
        return rep

    def rebuild_free_list(self):
        """Reconstruct this partition's free-block chain and inode cache from
        the inode table (the offline equivalent of V7 `icheck -s`). Every block
        in [isize, fsize) not referenced by any inode is chained free exactly as
        the kernel's bfree would build it. Cross-linked / out-of-range blocks are
        treated as USED (never re-chained), so a rebuild is always safe — but a
        non-empty cross_linked means the affected files' CONTENT is already
        damaged and no free-list repair can restore it. Returns the pre-rebuild
        report."""
        rep = self.check_blocks()
        used = set()
        nino = (rep.isize - 2) * INODES_PER_BLOCK
        for ino in range(1, nino + 1):
            inode = self.stat(ino)
            if inode is None or (not inode.is_file() and not inode.is_dir()):
                continue

            def visit(rel, indirect):
                if rep.isize <= rel < rep.fsize:
                    used.add(rel)

            self.walk_inode_blocks(inode, visit)

        # reset the superblock's free cache to EMPTY (s_nfree=0, s_free all 0:
        # an fs with no free blocks yet).  free_block below rebuilds the chain;
        # seeding nfree=1/free[0]=0 instead would push a bogus 0 onto the list
        # bottom (see free_block), which icheck flags as a dup of the boot block.
        # zero the running totals, and invalidate the inode cache
        sb = self._read_sb()
        struct.pack_into("<H", sb, SB_NFREE, 0)
        for i in range(NICFREE):
            self.write32(sb, SB_FREE + i * 4, 0)
        self.write32(sb, SB_TFREE, 0)
        struct.pack_into("<H", sb, SB_NINODE, 0)
        struct.pack_into("<H", sb, SB_TINODE, rep.free_inodes & 0xFFFF)
        sb[SB_FLOCK] = 0
        sb[SB_ILOCK] = 0
        self._touch_sb(sb)
        self._write_sb(sb)

        # chain every unused block, high to low, through the kernel's own bfree
        # (spills the cache into the freed block every NICFREE pushes). Freeing
        # descending means alloc hands out ascending block numbers — good locality.
        for b in range(rep.fsize - 1, rep.isize - 1, -1):
            if b not in used:
                self.free_block(b)
        return rep


def validate_name(name):
    if name in ("", ".", "..") or "/" in name or "\0" in name:
        raise FSError("invalid entry name %r" % name)
    if len(name.encode("latin-1", "replace")) > MAX_NAME:
        raise FSError("name %r exceeds %d bytes" % (name, MAX_NAME))


# -- partition discovery -------------------------------------------------

def probe_partition_at(image, part_off):
    """Return a CoherentFS if a valid filesystem starts at block `part_off`,
    else None. Accepts a freshly-made, otherwise-empty partition: the test is
    a plausible superblock plus a root inode whose directory begins with the
    canonical '.' and '..' entries (a fresh mkfs root is exactly those two)."""
    if (part_off + 3) * BLOCK > len(image):
        return None
    sb = image[(part_off + 1) * BLOCK:(part_off + 2) * BLOCK]
    isize = struct.unpack_from("<H", sb, 0)[0]
    if isize < 2 or isize > 2000:
        return None
    fs = CoherentFS(image, part_off)
    root = fs.stat(ROOTINO)
    if root is None or not root.is_dir():
        return None
    # a real root directory is a nonzero multiple of the 16-byte dirent size
    if root.size == 0 or root.size % DIRENT_SIZE != 0 or root.size > 65536:
        return None
    body = fs.read_file(root)
    if len(body) < 2 * DIRENT_SIZE:
        return None
    dot_ino = struct.unpack_from("<H", body, 0)[0]
    dot = body[2:DIRENT_SIZE].split(b"\0", 1)[0]
    dotdot = body[DIRENT_SIZE + 2:2 * DIRENT_SIZE].split(b"\0", 1)[0]
    if dot != b"." or dotdot != b".." or dot_ino != ROOTINO:
        return None
    return fs


def scan_partitions(image):
    """All Coherent filesystems on the image, in partition order.

    C900 hard-disk images have no MBR; partitions sit at the fixed block
    offsets baked into the WD driver (WD_PARTITIONS). We probe each in turn
    and keep the ones that hold a filesystem (empty partitions included).
    A single-partition image (e.g. a floppy) has only WD_PARTITIONS[0] at
    block 0 fitting, so it naturally returns just that one filesystem."""
    nblocks = len(image) // BLOCK
    parts = []
    for bstart, _bcount in WD_PARTITIONS:
        if bstart + 3 > nblocks:
            break
        fs = probe_partition_at(image, bstart)
        if fs:
            parts.append(fs)
    return parts


def parse_etc_rc_mounts(root):
    """{pseudo-drive index: mount path} from /etc/rc `mount /dev/hdN /path`."""
    mounts = {0: "/"}
    rc = root.lookup("/etc/rc")
    if rc is None or not rc.is_file():
        return mounts
    for line in root.read_file(rc).decode("latin-1", "replace").splitlines():
        f = line.split()
        if len(f) < 3 or not f[0].endswith("mount"):
            continue
        i = f[1].rfind("hd")
        if i < 0:
            continue
        try:
            idx = int(f[1][i + 2:])
        except ValueError:
            continue
        mounts[idx] = f[2]
    return mounts


def open_image(image, part=None):
    parts = scan_partitions(image)
    if not parts:
        raise FSError("no Coherent filesystem found in image")
    if part is not None:
        if part < 0 or part >= len(parts):
            raise FSError("partition %d out of range (found %d)" % (part, len(parts)))
        return parts[part], parts
    root = parts[0]
    mounts = parse_etc_rc_mounts(root)
    for idx, sub in enumerate(parts[1:], start=1):
        mp = mounts.get(idx)
        if mp and mp != "/":
            root.mounts.append((mp, sub))
    return root, parts


# -- CLI ------------------------------------------------------------------

def ensure_dirs(fs, path, mkdirs):
    """Resolve directory `path`, optionally creating missing components."""
    node = fs.lookup(path)
    if node is not None:
        if not node.is_dir():
            raise FSError("%s exists and is not a directory" % path)
        return node
    if not mkdirs:
        raise FSError("directory %s not found (use --mkdirs to create it)" % path)
    parts = [p for p in path.split("/") if p and p != "."]
    cur_path = ""
    for comp in parts:
        parent = fs.lookup(cur_path if cur_path else "/")
        cur_path += "/" + comp
        node = fs.lookup(cur_path)
        if node is None:
            node = fs.create_dir(parent, comp)
            print("  mkdir %s (ino %d)" % (cur_path, node.ino))
        elif not node.is_dir():
            raise FSError("%s exists and is not a directory" % cur_path)
    return fs.lookup(path)


def insert_one(fs, host_path, image_path, mode, mkdirs):
    with open(host_path, "rb") as f:
        data = f.read()
    image_path = "/" + "/".join(p for p in image_path.split("/") if p and p != ".")
    dir_path, _, name = image_path.rpartition("/")
    if not dir_path:
        dir_path = "/"
    validate_name(name)

    existing = fs.lookup(image_path)
    if existing is not None:
        if existing.is_dir():
            raise FSError("%s is a directory" % image_path)
        if not existing.is_file():
            raise FSError("%s exists and is not a regular file" % image_path)
        existing.owner.write_file(existing, data)
        if mode is not None:
            existing.mode = (mode & 0o7777) | S_IFREG
            existing.owner.write_inode(existing)
        print("  overwrote %s (ino %d, %d bytes)" % (image_path, existing.ino, len(data)))
        return

    parent = ensure_dirs(fs, dir_path, mkdirs)
    inode = fs.create_file(parent, name, data, mode if mode is not None else 0o644)
    print("  created %s (ino %d, %d bytes)" % (image_path, inode.ino, len(data)))


def norm_img(path):
    """Normalise an in-image path to an absolute, '.'-free '/'-path."""
    return "/" + "/".join(p for p in path.replace("\\", "/").split("/")
                          if p and p != ".")


def img_basename(path):
    return norm_img(path).rpartition("/")[2]


def copy_in(fs, host_path, image_path, mode, mkdirs):
    """Copy a host file or directory tree into the image at image_path."""
    image_path = norm_img(image_path)
    if os.path.isdir(host_path):
        # create the directory (and any parents) in the image, then recurse
        ensure_dirs(fs, image_path, True)
        for name in sorted(os.listdir(host_path)):
            copy_in(fs, os.path.join(host_path, name),
                    image_path + "/" + name, mode, mkdirs)
    elif os.path.isfile(host_path):
        insert_one(fs, host_path, image_path, mode, mkdirs)
    else:
        print("  skip %s (not a regular file or directory)" % host_path)


def copy_out(fs, image_path, host_path):
    """Copy an in-image file or directory tree out to the host at host_path."""
    image_path = norm_img(image_path)
    node = fs.lookup(image_path)
    if node is None:
        raise FSError("not found in image: %s" % image_path)
    if node.is_dir():
        os.makedirs(host_path, exist_ok=True)
        for ino, name in sorted(node.owner.read_dir(node), key=lambda e: e[1]):
            # recurse by path so mount points are followed like `lookup` does
            copy_out(fs, image_path + "/" + name, os.path.join(host_path, name))
    elif node.is_file():
        parent = os.path.dirname(host_path)
        if parent:
            os.makedirs(parent, exist_ok=True)
        data = node.owner.read_file(node)
        with open(host_path, "wb") as f:
            f.write(data)
        print("  read %s -> %s (%d bytes)" % (image_path, host_path, len(data)))
    else:
        print("  skip %s (not a regular file or directory, mode %06o)"
              % (image_path, node.mode))


def cmd_ls(fs, path):
    node = fs.lookup(path)
    if node is None:
        raise FSError("path not found: %s" % path)
    if not node.is_dir():
        print("%8d  %s" % (node.size, path))
        return
    for ino, name in sorted(node.owner.read_dir(node), key=lambda e: e[1]):
        child = node.owner.stat(ino)
        if child is None:
            print("    ?     %s (dangling ino %d)" % (name, ino))
            continue
        kind = "d" if child.is_dir() else ("-" if child.is_file() else "?")
        print("%s %06o %8d  %s" % (kind, child.mode & 0o7777, child.size, name))


def inode_paths(fs):
    """Map every inode reachable from this partition's root to a '/'-path,
    by recursive directory walk (single partition, no mounts). Used to name
    damaged inodes in fsck output."""
    out = {fs.root_ino: "/"}

    def walk(ino, prefix):
        node = fs.stat(ino)
        if node is None or not node.is_dir():
            return
        for cino, name in fs.read_dir(node):
            if cino in out:                # hardlink / cycle guard
                continue
            p = prefix + "/" + name
            out[cino] = p
            walk(cino, p)

    walk(fs.root_ino, "")
    return out


def run_fsck(fs):
    """Repair the given partition: rebuild its free list from the inode table
    and report cross-linked / out-of-range blocks. The image bytearray is
    modified in place; the caller writes it back. Returns True if content
    damage (cross-linked / out-of-range) was found."""
    rep = fs.rebuild_free_list()
    print("fsck: partition at block %d: %s" % (fs.part_off, rep))
    names = inode_paths(fs)
    for blk, inos in sorted(rep.cross_linked.items()):
        paths = [names.get(i, "?(unlinked)") for i in inos]
        print("  cross-linked block %d: inodes %s %s" % (blk, inos, paths))
    for ino, blks in sorted(rep.out_of_range.items()):
        print("  inode %d (%s) has out-of-range blocks %s"
              % (ino, names.get(ino, "?(unlinked)"), blks))
    return rep.damaged()


def parse_mode(s):
    try:
        return int(s, 8)
    except ValueError:
        raise argparse.ArgumentTypeError("mode must be octal, e.g. 755")


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Copy files between the host and a Coherent (C900) disk image.",
        epilog="FILE is a bare path (placed under --dest/<basename>) or SRC=DST; "
               "SRC is always the source, DST the destination. Directories copy "
               "recursively.")
    ap.add_argument("image", help="disk image (HD or floppy)")
    ap.add_argument("files", nargs="*", metavar="FILE",
                    help="path to copy, or SRC=DST")
    mode_grp = ap.add_mutually_exclusive_group()
    mode_grp.add_argument("--write", action="store_true",
                          help="copy host -> image (each FILE is a host path / SRC=IMGDST)")
    mode_grp.add_argument("--read", action="store_true",
                          help="copy image -> host (each FILE is an image path / SRC=HOSTDST)")
    mode_grp.add_argument("--fsck", action="store_true",
                          help="check and repair the partition (rebuild its free list "
                               "from the inode table); uses --partition (default 0)")
    ap.add_argument("--dest", default=None, metavar="DIR",
                    help="destination directory for bare FILE args "
                         "(--write default /, --read default current dir)")
    ap.add_argument("--mode", type=parse_mode, default=None, metavar="OCTAL",
                    help="[--write] permission bits for inserted files (default 644 for "
                         "new files; existing files keep their mode unless this is given)")
    ap.add_argument("--mkdirs", action="store_true",
                    help="[--write] create missing destination directories")
    ap.add_argument("--partition", "--part", type=int, default=None, metavar="N",
                    dest="partition",
                    help="operate on partition N directly (0=hd0, 1=hd1, ...), "
                         "bypassing the fragile /etc/rc mount decoding")
    ap.add_argument("--out", metavar="PATH",
                    help="[--write] write the modified image here instead of in place")
    ap.add_argument("--ls", metavar="PATH",
                    help="list an in-image directory (after any copy) and exit")
    args = ap.parse_intermixed_args(argv)

    if not (args.write or args.read or args.fsck or args.ls):
        ap.error("nothing to do: one of --write, --read, --fsck, or --ls is required")
    if (args.write or args.read) and not args.files:
        ap.error("--%s needs FILE arguments" % ("write" if args.write else "read"))
    if args.files and not (args.write or args.read):
        ap.error("FILE arguments require --write or --read")

    with open(args.image, "rb") as f:
        image = bytearray(f.read())

    # --fsck operates on the partition chosen by --partition (default 0),
    # standalone — no /etc/rc mount grafting. Other modes default to hd0+mounts.
    part = args.partition
    if args.fsck and part is None:
        part = 0
    fs, parts = open_image(image, part)
    print("%s: %d partition(s), using %s at block %d (root ino %d)" %
          (args.image, len(parts),
           ("partition %d" % part) if part is not None else "hd0",
           fs.part_off, fs.root_ino))
    for mp, sub in fs.mounts:
        print("  mount %s -> partition at block %d" % (mp, sub.part_off))

    rc = 0
    if args.fsck:
        damaged = run_fsck(fs)
        out_path = args.out or args.image
        with open(out_path, "wb") as f:
            f.write(image)
        print("image updated: %s (%d bytes)" % (out_path, len(image)))
        if damaged:
            print("WARNING: cross-linked/out-of-range blocks mean file CONTENT damage; "
                  "affected files should be restored from a known-good source")
            rc = 3

    if args.write:
        dest = args.dest if args.dest is not None else "/"
        for spec in args.files:
            if "=" in spec:
                host_path, image_path = spec.split("=", 1)
            else:
                host_path = spec
                image_path = dest.rstrip("/") + "/" + os.path.basename(spec.rstrip("/\\"))
            copy_in(fs, host_path, image_path, args.mode, args.mkdirs)
        out_path = args.out or args.image
        with open(out_path, "wb") as f:
            f.write(image)
        print("wrote %s (%d bytes)" % (out_path, len(image)))

    elif args.read:
        dest = args.dest if args.dest is not None else "."
        for spec in args.files:
            if "=" in spec:
                image_path, host_path = spec.split("=", 1)
            else:
                image_path = spec
                host_path = os.path.join(dest, img_basename(spec))
            copy_out(fs, image_path, host_path)

    if args.ls:
        cmd_ls(fs, args.ls)
    return rc


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FSError, OSError) as e:
        print("error: %s" % e, file=sys.stderr)
        sys.exit(1)
