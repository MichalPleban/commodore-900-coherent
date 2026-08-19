# Makefile - rebuild the Coherent Z8001 libraries and installed headers.
#
# Builds into the build/ staging tree (see CLAUDE.md):
#   build/root/usr/include/  - installed system headers
#   build/root/lib/          - crts0.o dtoa.o libc.a libm.a libmp.a liby.a
#   build/obj/               - intermediate .o/.s, mirroring src/
#
# Run with GNU make (recipes use an sh-compatible shell + coreutils).

# A failed recipe must not leave a half-made target behind: the .sl rules
# in particular link and THEN stamp (mkslib.py), and an unstamped library
# poisons every client link that considers it up to date.
.DELETE_ON_ERROR:

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
CC     = z8001-coherent-cc
AS     = z8001-coherent-as
LD     = z8001-coherent-ld
CPP    = z8001-coherent-cpp
AR     = coherent-ar
RANLIB = coherent-ranlib
YACC   = coherent-yacc

# K&R leniency (-ftraditional), Coherent's readonly==const spelling, and the
# in-tree headers.  See COHERENT.md for why each is needed.
# (The cc DRIVER predefines Z8001 for cpp; only the standalone cpp binary does
# not.  <l.out.h>-style #ifdef Z8001 headers resolve correctly without help.)
CFLAGS  = -O -ftraditional -Dreadonly=const -I$(INCSRC)
ASFLAGS = -g

# Archive $^ into a fresh $@ in basename-alphabetical order, then index it.
# The order is load-bearing for libc's optional-float-printf trick: the real
# FP formatter (dtefg.o, defines _dtefg_ and _dtoa_) must precede printf.o /
# scanf.o, and the "No floating point!" stub (sdtoa.o, also defines _dtefg_)
# must FOLLOW them, so Coherent's single-pass ld picks the stub by default and
# the real routine only when a bare _dtoa_ reference (the standalone dtoa.o, or
# -u _dtoa_) is linked before libc.a.  Alphabetical order (dtefg < printf <
# scanf < sdtoa) satisfies this; do NOT regroup the FP objects together.
# See COHERENT.md.
define ar-sorted
	@mkdir -p $(dir $@)
	@rm -f $@
	@# tr -d '\r': depending on PATH, `sort` may be Windows System32 sort.exe and
	@# `cut` may be Gow's cut, both of which write CRLF; a stray \r on a path would
	@# make coherent-ar try to open "name.o\r" and fail.  Strip it defensively.
	@objs=`for o in $^; do echo "$$(basename $$o)|$$o"; done | sort | cut -d'|' -f2 | tr -d '\r'`; \
	 $(AR) cr $@ $$objs
	$(RANLIB) $@
endef

# ---------------------------------------------------------------------------
# Layout
# ---------------------------------------------------------------------------
SRC    = src
INCSRC = $(SRC)/include
UL     = $(SRC)/userland

BUILD  = build
OBJ    = $(BUILD)/obj
ROOT   = $(BUILD)/root
LIBDIR = $(ROOT)/lib
INCDIR = $(ROOT)/usr/include

# Command install locations inside the staging image.  The split between
# /bin, /usr/bin, /etc and /usr/lib is taken from the original Coherent
# filesystem listing (recovered; the cmd/* makefiles do not encode it).
BINDIR    = $(ROOT)/bin
ETCDIR    = $(ROOT)/etc
USRBINDIR = $(ROOT)/usr/bin
USRLIBDIR = $(ROOT)/usr/lib
GAMESDIR  = $(ROOT)/usr/games

CMDS = $(UL)/cmd

# ---------------------------------------------------------------------------
# Installed headers
# ---------------------------------------------------------------------------
HDRS       := $(wildcard $(INCSRC)/*.h $(INCSRC)/sys/*.h)
INC_TARGET := $(patsubst $(INCSRC)/%,$(INCDIR)/%,$(HDRS))

# ---------------------------------------------------------------------------
# Header dependencies
# ---------------------------------------------------------------------------
# The compile rules see one .c at a time and the compiler emits no dependency
# information, so nothing here can know which headers a source pulled in.  Each
# subsystem therefore declares its header set ($(HDRS) here, $(KHDRS), $(HRHDRS),
# $(HRGUIHDRS) below) and EVERY object of that subsystem depends on ALL of it.
#
# Deliberately coarse: editing a header rebuilds more than it strictly must,
# which costs a minute of compiling.  The alternative failure -- an object left
# behind, compiled against a struct layout or a manifest constant that no longer
# exists -- is silent, and produces a system that builds cleanly and then
# misbehaves at run time.  That has cost hours more than once (a stale kernel
# object after a param.h change; a GUI client holding an HRAPP one field short of
# the hr_open() that writes it), so the trade is not close.

# ---------------------------------------------------------------------------
# libc - compile every .c; assemble a .s only when there is no matching .c
# (the paired .s files are stale output of the original compiler; the .s-only
# files are the hand-written primitives: syscall stubs, strcmp, setjmp, ...).
# ---------------------------------------------------------------------------
LIBC_SUB := crt gen stdio sys
libc_c   := $(wildcard $(foreach d,$(LIBC_SUB),$(UL)/lib/libc/$(d)/*.c))
libc_s   := $(wildcard $(foreach d,$(LIBC_SUB),$(UL)/lib/libc/$(d)/*.s))
libc_sonly := $(filter-out $(addsuffix .s,$(basename $(libc_c))),$(libc_s))

LIBC_OBJ := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(libc_c)) \
            $(patsubst $(SRC)/%.s,$(OBJ)/%.o,$(libc_sonly))

# libm / libmp / liby / libfs - all .c in the directory (per their run/compile scripts)
libm_c   := $(wildcard $(UL)/lib/libm/*.c)
libmp_c  := $(wildcard $(UL)/lib/libmp/*.c)
liby_c   := $(wildcard $(UL)/lib/liby/*.c)
libfs_c  := $(wildcard $(UL)/lib/libfs/*.c)
LIBM_OBJ  := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(libm_c))
LIBMP_OBJ := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(libmp_c))
LIBY_OBJ  := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(liby_c))
LIBFS_OBJ := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(libfs_c))

# ---------------------------------------------------------------------------
# Top-level targets
# ---------------------------------------------------------------------------
CRT   = $(LIBDIR)/crts0.o
DTOA  = $(LIBDIR)/dtoa.o
LIBC  = $(LIBDIR)/libc.a
# the shared C library (kernel LF_SLIB slot 0); build rule after the hr
# section (it reuses SLCRT).  Commands link it via $(SLREF) in the `link'
# define; the rescue set overrides back to static.
LIBC_SL = $(LIBDIR)/libc.sl
LIBM  = $(LIBDIR)/libm.a
# the reference image keeps libmp.a in /usr/lib, not /lib
LIBMP = $(USRLIBDIR)/libmp.a
LIBY  = $(LIBDIR)/liby.a
# libfs: shared file-system access for the check tools (icheck/dcheck/ncheck).
LIBFS = $(LIBDIR)/libfs.a

LIBS = $(CRT) $(DTOA) $(LIBC) $(LIBM) $(LIBMP) $(LIBY) $(LIBFS)

.PHONY: all headers libs cmds kernel dist man image floppy hr hrgui clean
all: headers libs cmds kernel dist man image
headers: $(INC_TARGET)
libs: $(LIBS)

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------
$(INCDIR)/%.h: $(INCSRC)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

$(OBJ)/%.o: $(SRC)/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

# Generated sources (e.g. a yacc y.tab.c) live in the obj tree, not src/.
$(OBJ)/%.o: $(OBJ)/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# crt objects: standalone in $(LIBDIR) (build/root/lib), assembled from csu/
$(LIBDIR)/crts0.o: $(UL)/lib/csu/crts0.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(LIBDIR)/dtoa.o: $(UL)/lib/csu/dtoa.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(LIBDIR)/libc.a: $(LIBC_OBJ)
	$(ar-sorted)

$(LIBDIR)/libm.a: $(LIBM_OBJ)
	$(ar-sorted)

$(USRLIBDIR)/libmp.a: $(LIBMP_OBJ)
	$(ar-sorted)

$(LIBDIR)/liby.a: $(LIBY_OBJ)
	$(ar-sorted)

$(LIBDIR)/libfs.a: $(LIBFS_OBJ)
	$(ar-sorted)

# ===========================================================================
# Userland commands  (cmd/*)
# ===========================================================================
# Install locations (/bin, /usr/lib, /etc) come from the original Coherent
# filesystem listing.  Each binary links crts0.o + its objects + libc.a; the
# three FP-formatting programs additionally link dtoa.o (the real _dtefg_/
# _dtoa_ routine) so they are not stuck with libc's "No floating point!" stub.
#
# The grammar-based commands (awk expr find test) are generated with
# coherent-yacc; find/test link -ly, awk links -lm, and test also ships as `['.
#
# NOT built here:
#   * cmd sources with no shipped binary:  Ncron rsh, knapsack extras
#                           (only enroll/xencode/xdecode ship), opr, spellin, psh.
#     (crypt is built to /bin even though it is absent from the reference image,
#      where it was omitted for US crypto-export reasons.)
#   * mail  - built from 7mail.c (the shipped variant); xmail.c is the unused
#             alternate.  See the $(BINDIR)/mail rule below.
#
# History (kept for reference):
#   * ed (-DCOHERENT) and as (-Dasm=Asm, its main routine is named asm()) build
#     via Coherent-idiom -D flags; tsort and mkfs needed one-character source
#     typo fixes.
#   * init.c and umount.c had bad-sector NUL corruption; the lost spans were
#     reconstructed from disasm/init.asm and disasm/umount.asm.
#   * sh nroff grep lpr(lpd/print) restor bc once failed on pre-ANSI constructs
#     (anonymous struct/union members, tag punning, extern scope, a `>>` codegen
#     gap); they build now that the compiler and sources were fixed.
#   * dump chmod/chown and setuid bits are not reproduced (Windows staging
#     tree); load-order/perms are applied when the image is packed.
#
# link: $(1) = objects/libs to place between crts0.o and libc.a.
# -s strips the symbol/debug table (the .o files carry it because the libc/crt
# and command objects are assembled with ASFLAGS=-g); the installed binaries
# don't need it, and it dominates their size.
# LDNFLAGS: per-target extra ld flags (target-specific, e.g. `-n` to bind a
# binary shared-text so all instances -- and every fork -- reuse ONE in-core
# copy of its text; the kernel keys the shared segment on the inode).  Safe
# only for binaries whose SHRD is empty or genuinely read-only.
# SLREF: the shared library reference.  Commands link against
# /lib/libc.sl -- ld imports its whole export set as absolute addresses
# (segment 0x34 text, segment 1 per-process data) and the trailing
# static libc.a supplies only what the .sl omits (crypt,
# getpwent/getgrent, getwd, getpass, profiling).  init loads the .sl at
# boot from the path the kernel passes in its argument vector (icode in
# md.s).  Deployed Aug 2026 (disk -39%; the resident ~25K holder is the
# accepted price).  The RESCUE SET overrides below keep boot/repair
# fully static: set SLREF empty there so a damaged or absent libc.sl
# can never take down single-user or the repair tools.
SLREF = $(LIBC_SL)
define link
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(1) $(SLREF) $(LIBC)
endef

# --- single-file commands -> /bin ------------------------------------------
BIN_CMDS := ac ar at bad banner basename c cal cat check chgrp chmod chown \
	cmp col comm conv cp cpdir crypt date dd deroff df diff3 du echo file \
	from head help join kill l lc learn ln login look ls m4 mesg mkdir \
	msg mv newgrp nm od passwd pr prep prof pwd quot ranlib rev rm \
	rmdir sa scat size sleep sort split strip stty su sum sync tail tar tee \
	time touch tr tty typo uniq version wc who write yes

# icheck/dcheck/ncheck are built separately (they link libfs.a) -- see below.
FS_CMDS := icheck dcheck ncheck

# --- single-file commands -> /etc ------------------------------------------
ETC_CMDS := accton clri cron fdformat getty init mkfs mknod mkproto reboot umount update wall

# system utilities that pull in kernel headers (struct proc, drvcon.h, mount.h).
KERN_ETC_CMDS := load mount uload

# --- single-file commands -> /usr/games ------------------------------------
GAMES_CMDS := fortune moo

BIN_TARGETS := $(addprefix $(BINDIR)/,$(BIN_CMDS))
ETC_TARGETS := $(addprefix $(ETCDIR)/,$(ETC_CMDS))
KERN_ETC_TARGETS := $(addprefix $(ETCDIR)/,$(KERN_ETC_CMDS))
GAMES_TARGETS := $(addprefix $(GAMESDIR)/,$(GAMES_CMDS))

# The static rescue set: bootable and repairable with no shared library.
$(BINDIR)/sh $(BINDIR)/check $(BINDIR)/sync \
$(BINDIR)/icheck $(BINDIR)/dcheck $(BINDIR)/ncheck \
$(ETCDIR)/init $(ETCDIR)/clri $(ETCDIR)/mkfs $(ETCDIR)/mknod \
$(ETCDIR)/reboot $(ETCDIR)/umount $(ETCDIR)/update \
$(ETCDIR)/mount $(ETCDIR)/load $(ETCDIR)/uload: SLREF :=

# Any libc.sl change must relink every shared client (the ABI rule: clients
# bind absolute addresses).  Harmless extra prerequisite for the rescue set.
$(BIN_TARGETS) $(ETC_TARGETS) $(KERN_ETC_TARGETS) $(GAMES_TARGETS) \
$(BINDIR)/ps $(BINDIR)/mem $(BINDIR)/factor $(BINDIR)/units $(BINDIR)/mail \
$(BINDIR)/icheck $(BINDIR)/dcheck $(BINDIR)/ncheck \
$(USRLIBDIR)/atrun $(USRLIBDIR)/diff3 $(USRLIBDIR)/diffh $(USRLIBDIR)/lpd \
$(USRLIBDIR)/spell $(USRBINDIR)/compress $(USRBINDIR)/kermit \
$(BINDIR)/as $(BINDIR)/awk $(BINDIR)/bc $(BINDIR)/cu $(BINDIR)/dc \
$(BINDIR)/diff $(BINDIR)/dump $(BINDIR)/dumpdate $(BINDIR)/dumpdir \
$(BINDIR)/ed $(BINDIR)/egrep $(BINDIR)/enroll $(BINDIR)/expr $(BINDIR)/find \
$(BINDIR)/grep $(BINDIR)/ld $(BINDIR)/lex $(BINDIR)/lpr $(BINDIR)/lpskip \
$(BINDIR)/make $(BINDIR)/me $(BINDIR)/nroff $(BINDIR)/restor $(BINDIR)/sed \
$(BINDIR)/sh $(BINDIR)/test $(BINDIR)/[ $(BINDIR)/tsort $(BINDIR)/xdecode \
$(BINDIR)/xencode $(BINDIR)/yacc: $(LIBC_SL)

$(BIN_TARGETS): $(BINDIR)/%: $(OBJ)/userland/cmd/%.o $(CRT) $(LIBC)
	$(call link,$<)

$(ETC_TARGETS): $(ETCDIR)/%: $(OBJ)/userland/cmd/%.o $(CRT) $(LIBC)
	$(call link,$<)

$(GAMES_TARGETS): $(GAMESDIR)/%: $(OBJ)/userland/cmd/%.o $(CRT) $(LIBC)
	$(call link,$<)

# (init embeds VERSION from <machine.h>; that -- like every other header in
# src/include -- is covered by the $(HDRS) prerequisite on the compile rules.)

$(addprefix $(OBJ)/userland/cmd/,$(addsuffix .o,$(KERN_ETC_CMDS))): CFLAGS += -I$(SRC)/kernel/h
$(KERN_ETC_TARGETS): $(ETCDIR)/%: $(OBJ)/userland/cmd/%.o $(CRT) $(LIBC)
	$(call link,$<)

# ps: reads the live proc table, so it needs the machine-specific kernel
# headers (proc.h/sched.h/seg.h/uproc.h).  The z8001/h copies must come first:
# the generic kernel/h/seg.h is byte-corrupted (see below), while z8001/h's is
# intact, and the z8001 struct layouts are the ones this build actually uses.
$(OBJ)/userland/cmd/ps.o: CFLAGS += -I$(SRC)/kernel/z8001/h -I$(SRC)/kernel/h
$(BINDIR)/ps: $(OBJ)/userland/cmd/ps.o $(CRT) $(LIBC)
	$(call link,$<)

# mem: reports physical memory usage by walking the kernel's in-core segment
# queue -- same /coherent + /dev/kmem mechanism (and the same header needs)
# as ps.
$(OBJ)/userland/cmd/mem.o: CFLAGS += -I$(SRC)/kernel/z8001/h -I$(SRC)/kernel/h
$(BINDIR)/mem: $(OBJ)/userland/cmd/mem.o $(CRT) $(LIBC)
	$(call link,$<)

# --- single-file commands needing floating point / other libs / other dir --
# factor: sqrt() -> libm, "%.0f" -> real dtoa formatter.
$(BINDIR)/factor: $(OBJ)/userland/cmd/factor.o $(CRT) $(DTOA) $(LIBM) $(LIBC)
	$(call link,$(DTOA) $< $(LIBM))

# units: "%g" -> real dtoa formatter.
$(BINDIR)/units: $(OBJ)/userland/cmd/units.o $(CRT) $(DTOA) $(LIBC)
	$(call link,$(DTOA) $<)

# mail: the interactive V7 mailer.  Its source is 7mail.c (the shipped variant;
# xmail.c is the unused alternate), so the target name differs from the basename
# and needs an explicit rule.  Installed setuid-root (perms applied at pack time).
$(BINDIR)/mail: $(OBJ)/userland/cmd/7mail.o $(CRT) $(LIBC)
	$(call link,$<)

# file-system check tools: each links libfs.a (the shared FS access layer and
# the inode->pathname engine).  libfs.a precedes libc.a so its references to
# libc (malloc/fprintf/strncpy/l3tol) resolve under Coherent's single-pass ld.
$(BINDIR)/icheck: $(OBJ)/userland/cmd/icheck.o $(CRT) $(LIBFS) $(LIBC)
	$(call link,$< $(LIBFS))
$(BINDIR)/dcheck: $(OBJ)/userland/cmd/dcheck.o $(CRT) $(LIBFS) $(LIBC)
	$(call link,$< $(LIBFS))
$(BINDIR)/ncheck: $(OBJ)/userland/cmd/ncheck.o $(CRT) $(LIBFS) $(LIBC)
	$(call link,$< $(LIBFS))

# atrun installs to /usr/lib; diff3 ships in both /bin and /usr/lib.
$(USRLIBDIR)/atrun: $(OBJ)/userland/cmd/atrun.o $(CRT) $(LIBC)
	$(call link,$<)
$(USRLIBDIR)/diff3: $(OBJ)/userland/cmd/diff3.o $(CRT) $(LIBC)
	$(call link,$<)

# --- multi-file commands ---------------------------------------------------
# as: assembler + Z8001 tables.  Its main routine is named asm(), which the
# modern front end reserves as the `asm` keyword -> rename the identifier.
as_obj := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,\
	$(wildcard $(CMDS)/as/*.c $(CMDS)/as/z8001/*.c))
$(as_obj): CFLAGS += -I$(CMDS)/as -I$(CMDS)/as/z8001 -Dasm=Asm
$(BINDIR)/as: $(as_obj) $(CRT) $(LIBC)
	$(call link,$(as_obj))

# cu: call-Unix serial dialer.
cu_obj := $(addprefix $(OBJ)/userland/cmd/cu/,cu.o cudld.o cun.o cuxcvr.o)
$(cu_obj): CFLAGS += -I$(CMDS)/cu
$(BINDIR)/cu: $(cu_obj) $(CRT) $(LIBC)
	$(call link,$(cu_obj))

# compress / uncompress / zcat: LZW file compressor, ported from Coherent 3.2.
# Built -DVIRTUAL -DBITS=16: at 16-bit codes the hash/code tables are far larger
# than one Z8001 data segment, so VIRTUAL pages them through a scratch file
# (a /tmp file by default -- the 3.2 /dev/ram path does not exist here; -w names
# an alternative, and is_fs.c guards -w'ing a raw device that holds a filesystem).
# Installs to /usr/bin; uncompress/zcat are hardlinks (behaviour keys off
# argv[0]), made at image-pack time from the manifest.  See
# src/userland/cmd/compress/PORT.md.
compress_obj := $(OBJ)/userland/cmd/compress/compress.o $(OBJ)/userland/cmd/compress/is_fs.o
$(compress_obj): CFLAGS += -DVIRTUAL -DBITS=16
$(USRBINDIR)/compress: $(compress_obj) $(CRT) $(LIBC)
	$(call link,$(compress_obj))

# bc / dc: share the multi-precision back end from cmd/bc; both link -lmp.
$(OBJ)/userland/cmd/bc/%.o: CFLAGS += -I$(CMDS)/bc
$(OBJ)/userland/cmd/dc/%.o: CFLAGS += -I$(CMDS)/bc -I$(CMDS)/dc
bc_obj := $(addprefix $(OBJ)/userland/cmd/bc/,bcmch.o bcmutil.o getnum.o \
	globals.o gram.o grmact.o interp.o lex.o main.o output.o putnum.o)
dc_bc  := $(addprefix $(OBJ)/userland/cmd/bc/,bcmch.o bcmutil.o getnum.o \
	globals.o output.o putnum.o)
dc_own := $(addprefix $(OBJ)/userland/cmd/dc/,dc.o dcsub.o undefined.o)
$(BINDIR)/bc: $(bc_obj) $(CRT) $(LIBMP) $(LIBC)
	$(call link,$(bc_obj) $(LIBMP))
$(BINDIR)/dc: $(dc_own) $(dc_bc) $(CRT) $(LIBMP) $(LIBC)
	$(call link,$(dc_own) $(dc_bc) $(LIBMP))

# diff -> /bin/diff ; diffh -> /usr/lib (both use diff2.o).
$(BINDIR)/diff: $(OBJ)/userland/cmd/diff/diff1.o $(OBJ)/userland/cmd/diff/diff2.o $(CRT) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/diff/diff1.o $(OBJ)/userland/cmd/diff/diff2.o)
$(USRLIBDIR)/diffh: $(OBJ)/userland/cmd/diff/diffh.o $(OBJ)/userland/cmd/diff/diff2.o $(CRT) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/diff/diffh.o $(OBJ)/userland/cmd/diff/diff2.o)

# dump: four binaries (dump/restor share discbuf.o), all -> /bin.
$(OBJ)/userland/cmd/dump/%.o: CFLAGS += -I$(CMDS)/dump
$(BINDIR)/dumpdate: $(OBJ)/userland/cmd/dump/dumpdate.o $(CRT) $(LIBC)
	$(call link,$<)
$(BINDIR)/dumpdir: $(OBJ)/userland/cmd/dump/dumpdir.o $(CRT) $(LIBC)
	$(call link,$<)
$(BINDIR)/dump: $(OBJ)/userland/cmd/dump/dump.o $(OBJ)/userland/cmd/dump/discbuf.o $(CRT) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/dump/dump.o $(OBJ)/userland/cmd/dump/discbuf.o)
$(BINDIR)/restor: $(OBJ)/userland/cmd/dump/restor.o $(OBJ)/userland/cmd/dump/discbuf.o $(CRT) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/dump/restor.o $(OBJ)/userland/cmd/dump/discbuf.o)

# ed: temp-file setup is guarded by #if COHERENT (vs. RSX); select it.
ed_obj := $(addprefix $(OBJ)/userland/cmd/ed/,ed0.o ed1.o ed2.o ed3.o)
$(ed_obj): CFLAGS += -I$(CMDS)/ed -DCOHERENT
$(BINDIR)/ed: $(ed_obj) $(CRT) $(LIBC)
	$(call link,$(ed_obj))

# egrep / grep:
egrep_obj := $(addprefix $(OBJ)/userland/cmd/egrep/,egrep.o nfa.o equiv.o dfa.o search.o bits.o)
$(BINDIR)/egrep: $(egrep_obj) $(CRT) $(LIBC)
	$(call link,$(egrep_obj))
grep_obj := $(addprefix $(OBJ)/userland/cmd/grep/,grep1.o grep2.o)
$(BINDIR)/grep: $(grep_obj) $(CRT) $(LIBC)
	$(call link,$(grep_obj))

# knapsack: builds the public-key mail helpers enroll/xencode/xdecode (-lmp).
KN := $(OBJ)/userland/cmd/knapsack
$(KN)/%.o: CFLAGS += -I$(CMDS)/knapsack
$(BINDIR)/enroll: $(KN)/gpph.o $(KN)/knapsack.o $(KN)/public.o $(KN)/pkio.o $(KN)/enroll.o $(CRT) $(LIBMP) $(LIBC)
	$(call link,$(KN)/gpph.o $(KN)/knapsack.o $(KN)/public.o $(KN)/pkio.o $(KN)/enroll.o $(LIBMP))
$(BINDIR)/xencode: $(KN)/gpph.o $(KN)/knapsack.o $(KN)/public.o $(KN)/pkio.o $(KN)/xencode.o $(CRT) $(LIBMP) $(LIBC)
	$(call link,$(KN)/gpph.o $(KN)/knapsack.o $(KN)/public.o $(KN)/pkio.o $(KN)/xencode.o $(LIBMP))
$(BINDIR)/xdecode: $(KN)/gpph.o $(KN)/knapsack.o $(KN)/xdecode.o $(CRT) $(LIBMP) $(LIBC)
	$(call link,$(KN)/gpph.o $(KN)/knapsack.o $(KN)/xdecode.o $(LIBMP))

# ld: single translation unit all.c, sized by BREADBOX.
$(OBJ)/userland/cmd/ld/all.o: CFLAGS += -DBREADBOX=16384 -I$(CMDS)/ld
$(BINDIR)/ld: $(OBJ)/userland/cmd/ld/all.o $(CRT) $(LIBC)
	$(call link,$<)

# lex:
lex_obj := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(wildcard $(CMDS)/lex/*.c))
$(lex_obj): CFLAGS += -I$(CMDS)/lex
$(BINDIR)/lex: $(lex_obj) $(CRT) $(LIBC)
	$(call link,$(lex_obj))

# lpr / lpskip -> /bin ; lpd -> /usr/lib.
$(BINDIR)/lpr: $(OBJ)/userland/cmd/lpr/lpr.o $(CRT) $(LIBC)
	$(call link,$<)
$(BINDIR)/lpskip: $(OBJ)/userland/cmd/lpr/lpskip.o $(CRT) $(LIBC)
	$(call link,$<)
$(USRLIBDIR)/lpd: $(OBJ)/userland/cmd/lpr/lpd1.o $(OBJ)/userland/cmd/lpr/lpd2.o $(OBJ)/userland/cmd/lpr/print.o $(CRT) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/lpr/lpd1.o $(OBJ)/userland/cmd/lpr/lpd2.o $(OBJ)/userland/cmd/lpr/print.o)

# nroff:
nroff_obj := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(wildcard $(CMDS)/nroff/*.c))
$(nroff_obj): CFLAGS += -I$(CMDS)/nroff
$(BINDIR)/nroff: $(nroff_obj) $(CRT) $(LIBC)
	$(call link,$(nroff_obj))

# sed:
sed_obj := $(addprefix $(OBJ)/userland/cmd/sed/,sed0.o sed1.o sed2.o sed3.o)
$(BINDIR)/sed: $(sed_obj) $(CRT) $(LIBC)
	$(call link,$(sed_obj))

# sh: V3.4.5 (April 1993, from ../Versions/3.2_userspace_relic-d), the last
# Mark Williams state of the Bourne shell -- '#' comments, pushd/popd/dirs,
# ${VAR:=word} forms, POSIX exit statuses.  Uses the checked-in y.tab.c;
# YYMAXDEPTH raised per its Makefile (Comeau C++'s install script).
# Linked -n (shared text): a shell runs behind every terminal window and every
# fork of one otherwise carries a private copy of its ~28K text -- the single
# biggest lever on the 1 MiB machine's terminal count.
sh_obj := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(wildcard $(CMDS)/sh/*.c))
$(sh_obj): CFLAGS += -I$(CMDS)/sh -DVERSION='"V3.4.5"' -DYYMAXDEPTH=300
$(BINDIR)/sh: LDNFLAGS := -n
$(BINDIR)/sh: $(sh_obj) $(CRT) $(LIBC)
	$(call link,$(sh_obj))

# spell: /bin/spell is the wrapper SCRIPT (cmd/spell/spellcmd, installed
# verbatim); it drives the hashcheck binary /usr/lib/spell (spell.o + spell2.o).
# (spellin is not shipped.)
$(BINDIR)/spell: $(CMDS)/spell/spellcmd
	@mkdir -p $(dir $@)
	cp $< $@
$(USRLIBDIR)/spell: $(OBJ)/userland/cmd/spell/spell.o $(OBJ)/userland/cmd/spell/spell2.o $(CRT) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/spell/spell.o $(OBJ)/userland/cmd/spell/spell2.o)

# tsort:
tsort_obj := $(addprefix $(OBJ)/userland/cmd/tsort/,alc.o hash.o input.o logic.o main.o util.o)
$(BINDIR)/tsort: $(tsort_obj) $(CRT) $(LIBC)
	$(call link,$(tsort_obj))

# yacc: the parser generator, from y0..y6 (its own headers on the include path).
# yyparse.c in the same dir is the emitted-parser skeleton, installed verbatim
# to /lib via src/dist -- it is NOT linked into the yacc binary (the command's
# own makefile links only y?.o).  Distinct from the *host* coherent-yacc ($(YACC))
# used above to pre-generate the awk/expr/find/test grammars.
yacc_obj := $(addprefix $(OBJ)/userland/cmd/yacc/,y0.o y1.o y2.o y3.o y4.o y5.o y6.o)
$(yacc_obj): CFLAGS += -I$(CMDS)/yacc
$(BINDIR)/yacc: $(yacc_obj) $(CRT) $(LIBC)
	$(call link,$(yacc_obj))

# make: single translation unit make.c.
$(OBJ)/userland/cmd/make/make.o: CFLAGS += -I$(CMDS)/make
$(BINDIR)/make: $(OBJ)/userland/cmd/make/make.o $(CRT) $(LIBC)
	$(call link,$<)

# me: MicroEMACS screen editor (multi-file; recovered from the C900 hd3
# alien/emacs source).  Terminal/OS selection is compiled into ed.h (V7=1,
# ANSI=1), so no -D flags are needed.  The source Makefile named the output
# `emacs`; Coherent installs it as /bin/me.
me_obj := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(wildcard $(CMDS)/me/*.c))
$(me_obj): CFLAGS += -I$(CMDS)/me
$(BINDIR)/me: $(me_obj) $(CRT) $(LIBC)
	$(call link,$(me_obj))

# kermit: Columbia UNIX Kermit, single translation unit (recovered from hd3
# alien/kermit.c).  Config is compiled in (UNIXL=1, COHERENT=1).  Installs to
# /usr/bin.
$(USRBINDIR)/kermit: $(OBJ)/userland/cmd/kermit.o $(CRT) $(LIBC)
	$(call link,$<)

# --- yacc-based commands ---------------------------------------------------
# Cancel make's built-in implicit rule "%.c: %.y".  bc ships a checked-in,
# hand-tuned gram.c (built directly, using the bundled yy.h -- there is no
# y.tab.h generation step for it).  Without this, editing gram.y so it becomes
# newer than gram.c makes the built-in rule silently regenerate gram.c with the
# modern yacc (referencing a nonexistent y.tab.h), breaking the build.  The real
# yacc commands below use the explicit gen-yacc targets, not this implicit rule.
%.c: %.y

# coherent-yacc writes y.tab.c / y.tab.h into its working directory, so each
# grammar is run in its own obj subdir (with the .y given by absolute path).
# gen-yacc: $(1) = command subdir under cmd/.
define gen-yacc
	@mkdir -p $(OBJ)/userland/cmd/$(1)
	cd $(OBJ)/userland/cmd/$(1) && $(YACC) $(CURDIR)/$(CMDS)/$(1)/$(notdir $(wildcard $(CMDS)/$(1)/*.y))
endef

$(OBJ)/userland/cmd/awk/y.tab.c: $(CMDS)/awk/awk.y
	$(call gen-yacc,awk)
$(OBJ)/userland/cmd/expr/y.tab.c: $(CMDS)/expr/expr.y
	$(call gen-yacc,expr)
$(OBJ)/userland/cmd/find/y.tab.c: $(CMDS)/find/find.y
	$(call gen-yacc,find)
$(OBJ)/userland/cmd/test/y.tab.c: $(CMDS)/test/test.y
	$(call gen-yacc,test)

# awk: parser (y.tab.c) + awk0..awk6, links -lm.  awk1.c needs y.tab.h.
# awk.h uses `inline' as an identifier (renamed via -D).  Two source bugs that
# only ever compiled under the old global-struct-member namespace were fixed:
# awk.h declared `struct NODE *' for what is a `union NODE' (tag pun), and
# awk5.c wrote `n1->t_un.t_flag' for `n1->t_flag'.
awk_obj := $(OBJ)/userland/cmd/awk/y.tab.o \
	$(addprefix $(OBJ)/userland/cmd/awk/,awk0.o awk1.o awk2.o awk3.o awk4.o awk5.o awk6.o)
$(awk_obj): CFLAGS += -I$(CMDS)/awk -I$(OBJ)/userland/cmd/awk -Dinline=Inline
$(awk_obj): $(OBJ)/userland/cmd/awk/y.tab.c
$(BINDIR)/awk: $(awk_obj) $(CRT) $(LIBM) $(LIBC)
	$(call link,$(awk_obj) $(LIBM))

# expr: self-contained grammar (its own main/lexer); ignores the stale expr.c.
$(OBJ)/userland/cmd/expr/y.tab.o: CFLAGS += -I$(CMDS)/expr
$(BINDIR)/expr: $(OBJ)/userland/cmd/expr/y.tab.o $(CRT) $(LIBC)
	$(call link,$<)

# find / test: self-contained grammars, link -ly.  test also ships as `[`.
$(OBJ)/userland/cmd/find/y.tab.o: CFLAGS += -I$(CMDS)/find
$(BINDIR)/find: $(OBJ)/userland/cmd/find/y.tab.o $(CRT) $(LIBY) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/find/y.tab.o $(LIBY))
$(OBJ)/userland/cmd/test/y.tab.o: CFLAGS += -I$(CMDS)/test
$(BINDIR)/test $(BINDIR)/[: $(OBJ)/userland/cmd/test/y.tab.o $(CRT) $(LIBY) $(LIBC)
	$(call link,$(OBJ)/userland/cmd/test/y.tab.o $(LIBY))

# Aggregate of every command the tree can currently build.
CMD_TARGETS := $(BIN_TARGETS) $(ETC_TARGETS) $(KERN_ETC_TARGETS) $(GAMES_TARGETS) \
	$(addprefix $(BINDIR)/,$(FS_CMDS)) \
	$(BINDIR)/factor $(BINDIR)/units $(BINDIR)/mail $(USRLIBDIR)/atrun $(USRLIBDIR)/diff3 \
	$(BINDIR)/as $(BINDIR)/awk $(BINDIR)/bc $(BINDIR)/cu $(BINDIR)/dc \
	$(BINDIR)/expr $(BINDIR)/find $(BINDIR)/grep $(BINDIR)/ps $(BINDIR)/mem \
	$(BINDIR)/test $(BINDIR)/[ \
	$(BINDIR)/diff $(USRLIBDIR)/diffh \
	$(BINDIR)/dump $(BINDIR)/dumpdir $(BINDIR)/dumpdate $(BINDIR)/restor \
	$(BINDIR)/ed $(BINDIR)/egrep \
	$(BINDIR)/enroll $(BINDIR)/xencode $(BINDIR)/xdecode \
	$(BINDIR)/ld $(BINDIR)/lex \
	$(BINDIR)/lpr $(BINDIR)/lpskip $(USRLIBDIR)/lpd \
	$(BINDIR)/nroff $(BINDIR)/sed $(BINDIR)/sh $(BINDIR)/make $(BINDIR)/tsort $(BINDIR)/yacc \
	$(BINDIR)/me $(USRBINDIR)/kermit $(USRBINDIR)/compress \
	$(BINDIR)/spell $(USRLIBDIR)/spell

cmds: $(CMD_TARGETS)

# ===========================================================================
# Kernel  (src/kernel)  ->  Commodore 900 "WD" (Western Digital disk) build
# ===========================================================================
# Only the WD machine is built: the DTC `hd` disk and its HR kernel do not
# exist on production Commodores.  Two config variants come from the same
# kernel - hard-disk root (wdcon) -> build/root/coherent, and floppy root
# (fdcon) -> build/floppy/coherent.  Both video drivers are built as loadable
# /drv modules (lrtty = low-res 6845 text, hrtty = hi-res bitmap), plus the
# loadable swapper /etc/swap.
#
# Toolchain/source notes (see COHERENT.md):
#   * Kernel C uses the userland K&R leniency PLUS -DK1 -DNLD -DFIVEINCH -UDDT,
#     with the kernel headers FIRST on the include path (z8001/h then h) so they
#     win over src/include (which supplies sys/types.h etc.).
#   * The hand-written .s (md.s, mmas.s, scroll.s) use cpp directives; they are
#     preprocessed with `$(CPP) -P` (the -P drops the `# line` markers the
#     assembler rejects) before $(AS).
#   * The swapper and video drivers are bound to a specific kernel image with
#     `ld -k<image>` (reads its absolute symbols); they link against
#     build/root/coherent.
#   * Source fixes for the original compiler's global struct-member namespace:
#     alloc.h (anonymous union), fs1.c (qualified pipe-inode members); fdcon.c
#     lost its DTC `hdcon` entry.  kbtab.c compiles with -DCCE=1.

KSRC    = $(SRC)/kernel
KINC    = -I$(KSRC)/z8001/h -I$(KSRC)/h
# Every kernel header, for the dependency line below the object lists.  The
# driver subdirectories carry their own (lrtty/hrtty device headers).
KHDRS  := $(wildcard $(KSRC)/h/*.h $(KSRC)/z8001/h/*.h $(KSRC)/drv/*.h \
	$(KSRC)/z8001/drv/*.h $(KSRC)/z8001/drv/*/*.h)
KDEF    = -DK1 -DNLD -DFIVEINCH -UDDT
# Kernel headers precede $(INCSRC) so they are not shadowed by src/include.
KCFLAGS = -O -ftraditional -Dreadonly=const $(KINC) -I$(INCSRC) $(KDEF)
KUNDEF  = -u putchar_ -u debflag_ -u timeout_ -u ttopen_
KRELOC  = -R 0x30000000
# Symbol flags for the symboled $(KSYM) intermediate (the ld -k source; see the
# kernel-image rules below).  -X discards compiler-internal (L-prefixed) local
# symbols but keeps globals - ld -k needs the globals to link the swapper and
# drivers - shedding the ~2300 dead locals the -g assembly emits (~118K -> ~69K,
# leaving the original's ~10.5K global table).  The shipped kernels are linked
# separately with -s (no symbol table at all); the .o files keep their full
# symbols for the disassembly-diff checks.
KSTRIP  = -X

DISTDIR  = $(BUILD)/dist
DRVDIR   = $(ROOT)/drv
# floppy staging tree (bootable floppy kernel + the minimal system)
FLOPPYDIR = $(BUILD)/floppy

# libcoh.a: machine-independent core + shared tty/ct line disciplines.
COH_OBJS := $(addprefix $(OBJ)/kernel/coh/,\
	alloc.o bio.o clist.o clock.o exec.o fd.o fs1.o fs2.o fs3.o main.o \
	misc.o null.o pipe.o printf.o proc.o seg.o sig.o sys1.o sys2.o sys3.o \
	tab.o timeout.o var.o) \
	$(OBJ)/kernel/drv/tty.o $(OBJ)/kernel/drv/ct.o
# libcmdr.a: Commodore machine-dependent glue + the al serial driver.  (pty
# and lp are loadable /drv modules - see PTY_OBJS/LP_OBJS below - not
# resident.)
CMDR_OBJS := $(addprefix $(OBJ)/kernel/z8001/,\
	drv/al.o src/commodore.o src/console.o src/ddt.o src/trap.o)

LIBCOH  := $(OBJ)/kernel/libcoh.a
LIBCMDR := $(OBJ)/kernel/libcmdr.a
KMD     := $(OBJ)/kernel/z8001/src/md.o
KWD     := $(OBJ)/kernel/z8001/drv/wd.o
KSWAP   := $(OBJ)/kernel/ker/swap.o
# Machine config objects: root/swap device selection, compiled from z8001/cfg/.
WDCON   := $(OBJ)/kernel/z8001/cfg/wdcon.o
FDCON   := $(OBJ)/kernel/z8001/cfg/fdcon.o

# Loadable video drivers (l.out drivers installed in /drv).
LRTTY_OBJS := $(addprefix $(OBJ)/kernel/z8001/drv/lrtty/,kv.o v0.o mm.o kb.o kbtab.o mmas.o)
HRTTY_OBJS := $(addprefix $(OBJ)/kernel/z8001/drv/hrtty/,\
	hrterm1.o hrterm2.o gall.o scrollu.o subr.o tty.o kb.o kv.o kbtab.o scroll.o)
# Serial console: no display; forwards the console to the first serial line.
NOTTY_OBJS := $(OBJ)/kernel/z8001/drv/notty.o
# Pseudo-terminal driver: loadable /drv module (kept out of resident kernel).
PTY_OBJS := $(OBJ)/kernel/z8001/drv/pty.o
# Line printer driver: loadable /drv module, installed at boot by init (the
# icode argv in md.s lists /drv/lp after the console driver).
LP_OBJS := $(OBJ)/kernel/z8001/drv/lp.o

# Every kernel object -- C and the cpp-preprocessed assembly -- is rebuilt when
# any kernel header changes.  Without this a `make kernel` after editing (say)
# param.h relinks objects compiled against the OLD layout: it builds, boots, and
# then misbehaves, which reads as a code bug and is not one.
KERNEL_OBJS := $(COH_OBJS) $(CMDR_OBJS) $(KWD) $(KSWAP) $(WDCON) $(FDCON) \
	$(NOTTY_OBJS) $(PTY_OBJS) $(LP_OBJS) $(LRTTY_OBJS) $(HRTTY_OBJS) $(KMD)
$(KERNEL_OBJS): $(KHDRS)

# Kernel C objects use the kernel flags (replacing the userland CFLAGS).
$(COH_OBJS) $(CMDR_OBJS) $(KWD) $(KSWAP) $(WDCON) $(FDCON) $(NOTTY_OBJS) $(PTY_OBJS) $(LP_OBJS): CFLAGS = $(KCFLAGS)
# lrtty/ and hrtty/ sources also need their own directory on the include path.
$(LRTTY_OBJS): CFLAGS = $(KCFLAGS) -I$(KSRC)/z8001/drv/lrtty
$(HRTTY_OBJS): CFLAGS = $(KCFLAGS) -I$(KSRC)/z8001/drv/hrtty
# Both video drivers use the Commodore "Extended" keyboard table (kbtab.c): it
# maps the machine-specific keys - cursor, HELP, the W1-W3 mouse buttons and
# O1/O2 - that the plain IBM table (kbibmtab.c) lacks.  Its layout is selected
# with -DCCE=1 (which also defines the CCE keycode the table references).
$(OBJ)/kernel/z8001/drv/lrtty/kbtab.o:       CFLAGS = $(KCFLAGS) -I$(KSRC)/z8001/drv/lrtty -DCCE=1
$(OBJ)/kernel/z8001/drv/hrtty/kbtab.o: CFLAGS = $(KCFLAGS) -I$(KSRC)/z8001/drv/hrtty -DCCE=1
# trap.c uses the V7 trap-frame write-back idiom: trap() receives the saved
# user registers as parameters (r0..r15, see its REGS macro) and stores the
# syscall return value into parameter `r1`; the assembly caller (tsys in md.s)
# then reloads those stack slots into the user registers before iret.  Under
# -O, -xtemps promotes those parameters into hardware registers that are never
# spilled back to their stack home, and -xdce then discards the "dead" (never
# re-read) write-back stores -- so NO syscall return value reaches user space
# (fork() returns garbage to both parent and child -> init's spawned child
# skips execve, runs init's wait() loop, and the system never reaches a shell).
# Build trap.o unoptimised, matching the original kernel Makefile (which used
# no -O), so the register parameters stay in memory and the write-back works.
# trap.c is the only kernel source using this idiom.
$(OBJ)/kernel/z8001/src/trap.o: CFLAGS = -ftraditional -Dreadonly=const $(KINC) -I$(INCSRC) $(KDEF)

# Hand-written assembly: preprocess (cpp -P) then assemble.  These explicit
# rules override the generic $(OBJ)/%.o: $(SRC)/%.s pattern (which skips cpp).
# -DPARANOID matches the original build (src/kernel/z8001/src/Makefile,run:
# "/lib/cpp -E -DPARANOID -DNLD -UDDT md.s").  It gates md.s's one early
# "ei VI" in vint (interrupt handler prologue): with PARANOID the handler keeps
# vectored interrupts disabled until iret, so the CT3 clock is dismissed by clk
# before VI is re-enabled.  Without it, vint re-enables VI before clk clears the
# clock IP; on hardware the Z8036 IUS blocks the re-fire, but a model without IUS
# gating re-delivers the tick immediately -> clock-interrupt storm -> stack
# underflow -> segment-trap storm at the first idle/halt.  PARANOID is only
# referenced in md.s, so this is a no-op for mmas.s/scroll.s.
define kas
	@mkdir -p $(dir $@)
	$(CPP) -P -DPARANOID -DNLD -UDDT $(KINC) $< $@.i
	$(AS) $(ASFLAGS) -o $@ $@.i
endef
$(KMD): $(KSRC)/z8001/src/md.s
	$(kas)
$(OBJ)/kernel/z8001/drv/lrtty/mmas.o: $(KSRC)/z8001/drv/lrtty/mmas.s
	$(kas)
$(OBJ)/kernel/z8001/drv/hrtty/scroll.o: $(KSRC)/z8001/drv/hrtty/scroll.s
	$(kas)

# Kernel archives keep their listed (original) member order - not the
# FP-sorted ar-sorted used for libc.
define ar-kernel
	@mkdir -p $(dir $@)
	@rm -f $@
	$(AR) cr $@ $^
	$(RANLIB) $@
endef
$(LIBCOH): $(COH_OBJS)
	$(ar-kernel)
$(LIBCMDR): $(CMDR_OBJS)
	$(ar-kernel)

# Kernel images: -i (separate I&D), relocation base, and the four forced
# undefined refs the original build pulls in.  libc.a stands in for `-lc`.
# $(1) = symbol-table flags, $(2) = machine config object (wdcon/fdcon).
define link-kernel
	@mkdir -p $(dir $@)
	$(LD) $(1) -i -o $@ $(KRELOC) $(KUNDEF) $(KMD) $(2) $(KWD) $(LIBCMDR) $(LIBCOH) $(LIBC)
endef

# Symboled kernel: the -k symbol source for the loadables below.  NOT shipped
# (lives in build/obj); keeps globals (-X drops only the dead L-locals) so ld -k
# can resolve kernel symbol addresses.  Its text/data are byte-identical to the
# stripped shipped images - symbol-table flags never move code/data - so a
# loadable linked against $(KSYM) is equally valid against the stripped kernels.
KSYM := $(OBJ)/kernel/coherent
$(KSYM): $(KMD) $(WDCON) $(KWD) $(LIBCMDR) $(LIBCOH) $(LIBC)
	$(call link-kernel,$(KSTRIP),$(WDCON))

# Shipped HD kernel: keeps the global symbol table ($(KSTRIP) = -X, same as
# $(KSYM)) because ps and mem nlist("/coherent") at RUN time -- a stripped
# kernel broke both with "bad namelist".  The ~10.5K of symbols cost disk
# space only: boot loads the header-described segments, never the symbol
# table.  The floppy kernel stays -s (floppy space is real money and no one
# runs ps against it).
$(ROOT)/coherent: $(KMD) $(WDCON) $(KWD) $(LIBCMDR) $(LIBCOH) $(LIBC)
	$(call link-kernel,$(KSTRIP),$(WDCON))
$(FLOPPYDIR)/coherent: $(KMD) $(FDCON) $(KWD) $(LIBCMDR) $(LIBCOH) $(LIBC)
	$(call link-kernel,-s,$(FDCON))

# Loadable swapper + video drivers.  Linked with -k against the symboled $(KSYM)
# to read absolute kernel symbol addresses.  The wdcon/fdcon variants differ
# only in 9 bytes of config data (rootdev/swapdev/swapbot/swaptop values) and
# have identical layout, so these loadables are valid for BOTH kernels - a
# driver linked against either image comes out byte-identical.  (Note: -k drops
# a filename extension; $(KSYM) has none, so it is read as-is.)
# The swapper is exec'd (init's loadswp -> execve), so it needs only its entry
# point and is fully stripped (-s).  The /drv console drivers, however, are
# pulled in by the userland `load` command, which scans the driver's symbol
# table for the `<xx>con_' configuration symbol (kvcon_) to read its major
# index - so they must KEEP their global symbols.  Link them with -X (drop the
# dead L-prefixed locals, keep globals) instead of -s; a fully stripped driver
# makes `load' panic "Configuration table not found" and init exits.
#
# All these loadables must be EXECUTABLE: the swapper is run via execve, and the
# /drv modules are opened by `load' through the kernel's exlopen(), which rejects
# a file with no execute bit (EACCES, "permission denied") - `load' then fails to
# install the driver and the kvcon major (8) is left empty, so /dev/console has
# no console driver.  $(LD) leaves the output non-executable, so chmod +x here.
$(ROOT)/etc/swap: $(KSWAP) $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ -e swap_ $(KSWAP) -k$(KSYM)
	chmod +x $@
$(DRVDIR)/lrtty: $(LRTTY_OBJS) $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -X -o $@ $(LRTTY_OBJS) -k$(KSYM)
	chmod +x $@
$(DRVDIR)/hrtty: $(HRTTY_OBJS) $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -X -o $@ $(HRTTY_OBJS) -k$(KSYM)
	chmod +x $@
$(DRVDIR)/notty: $(NOTTY_OBJS) $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -X -o $@ $(NOTTY_OBJS) -k$(KSYM)
	chmod +x $@
$(DRVDIR)/pty: $(PTY_OBJS) $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -X -o $@ $(PTY_OBJS) -k$(KSYM)
	chmod +x $@
$(DRVDIR)/lp: $(LP_OBJS) $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -X -o $@ $(LP_OBJS) -k$(KSYM)
	chmod +x $@

KERNEL_TARGETS := $(ROOT)/coherent $(FLOPPYDIR)/coherent $(ROOT)/etc/swap \
	$(DRVDIR)/lrtty $(DRVDIR)/hrtty $(DRVDIR)/notty $(DRVDIR)/pty $(DRVDIR)/lp
kernel: $(KERNEL_TARGETS)

# ===========================================================================
# hr windowing system  (_graphics/hr -> build/root/{drv/hr, usr/hr/bin/*})
# ===========================================================================
# The recovered MGR-style window system: a loadable kernel driver (hr, major 7,
# a superset of hrtty - it owns the framebuffer, keyboard IRQ, polled mouse AND
# is the inter-process message switch), the screen-manager server (smgr), the
# desktop/window manager (dmgr), the client job library (jlib -> lib.j), and the
# graphics/clock managers + window clients.  See SMGR.md for the architecture and
# the exact source fixes this revival needed (a reconstructed hdr/jlib.h, a
# <con.h>->drvcon.h rename, two split nested struct-assignments, etc.).
#
# NOT part of `all`: this builds but cannot RUN yet - the emulator is headless
# (no video/mouse; see SMGR.md 8.4).  It is an opt-in target like `floppy`.
#
# Layout: sources under $(HRSRC)/<component>/, objects mirror to $(HROBJ)/, the
# server/clients install to $(HRBIN), the driver to $(DRVDIR)/hr.  Userland units
# use the same K&R leniency as the rest of userland plus the hr headers; the
# driver additionally needs the kernel headers/defines (it is kernel code).
HRSRC  = _graphics/hr/src
HRHDR  = _graphics/hr/hdr
HROBJ  = $(OBJ)/hr
HRBIN  = $(ROOT)/usr/hr/bin

HRCFLAGS  = -O -ftraditional -Dreadonly=const -I$(INCSRC) -I$(HRHDR)
# driver: kernel flags, with the hr headers LAST so the kernel headers win.
HRKCFLAGS = $(KCFLAGS) -I$(HRHDR)
# hr headers: hdr/ plus the system ones (and the kernel's, for the driver TU).
HRHDRS   := $(wildcard $(HRHDR)/*.h) $(HDRS) $(KHDRS)

# hr object pattern rules (the generic src/ rules don't match _graphics/).
$(HROBJ)/%.o: $(HRSRC)/%.c $(HRHDRS)
	@mkdir -p $(dir $@)
	$(CC) $(HRCFLAGS) -c $< -o $@
# hr assembly: block1/2 and small1/2 use cpp directives; cpp -P is a harmless
# pass-through for the rest.  (Mirrors the historical smgr .s.o rule.)
$(HROBJ)/%.o: $(HRSRC)/%.s $(HRHDRS)
	@mkdir -p $(dir $@)
	$(CPP) -P -I$(HRHDR) $< $@.i
	$(AS) $(ASFLAGS) -o $@ $@.i
# the driver translation unit (hr.c #includes hr2.c) needs the kernel flags.
$(HROBJ)/driver/hr.o: HRCFLAGS = $(HRKCFLAGS)

# --- screen manager (smgr) ---
# GOBJ -> lib.g (graphics library); LOBJ + BLTOBJ + lib.g -> the smgr binary.
# rmath.o (in LOBJ) is also bundled into lib.j and linked into dmgr.
SMGR_GOBJ := $(addprefix $(HROBJ)/smgr/,gctrl.o gpoint.o gline.o gtext.o gtext2.o gcoord.o stubs.o glftn.o)
SMGR_BLT  := $(addprefix $(HROBJ)/smgr/,ablt.o small1.o small2.o block1.o block2.o ptrmath.o)
SMGR_LOBJ := $(addprefix $(HROBJ)/smgr/,f2.o bitblt.o globals.o kev.o layer.o masks.o rmath.o sm_funcs.o smgr.o wmgr.o fcpy.o)
$(HROBJ)/smgr/lib.g: $(SMGR_GOBJ)
	$(ar-kernel)
$(HRBIN)/smgr: $(SMGR_BLT) $(SMGR_LOBJ) $(HROBJ)/smgr/lib.g $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(SMGR_BLT) $(SMGR_LOBJ) $(HROBJ)/smgr/lib.g $(LIBC)

# --- job library (jlib -> lib.j) ---
JLIB_OBJ := $(addprefix $(HROBJ)/jlib/jl,$(addsuffix .o,1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21)) \
	$(HROBJ)/jlib/jsend.o $(HROBJ)/jlib/job.o
$(HROBJ)/jlib/lib.j: $(JLIB_OBJ) $(HROBJ)/smgr/rmath.o
	$(ar-kernel)

# --- desktop / window manager (dmgr) ---
DESK_OBJ := $(addprefix $(HROBJ)/desk/,dpmath.o dalert.o dmenu1.o dmenu2.o dmesg.o dmouse.o \
	dopen.o drect.o dshell.o dtext.o dstretch.o f1.o f2.o main.o)
$(HRBIN)/dmgr: $(DESK_OBJ) $(HROBJ)/smgr/rmath.o $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(DESK_OBJ) $(HROBJ)/smgr/rmath.o $(LIBC)

# --- graphics manager + clients (link lib.j; -lm where the app uses it) ---
$(HRBIN)/gmgr: $(HROBJ)/graph/gmgr.o $(HROBJ)/jlib/lib.j $(CRT) $(LIBM) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HROBJ)/graph/gmgr.o $(HROBJ)/jlib/lib.j $(LIBM) $(LIBC)
$(HRBIN)/gsh: $(HROBJ)/graph/gsh.o $(HROBJ)/jlib/lib.j $(CRT) $(LIBM) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HROBJ)/graph/gsh.o $(HROBJ)/jlib/lib.j $(LIBM) $(LIBC)
$(HRBIN)/clock: $(HROBJ)/clock/cmgr.o $(HROBJ)/clock/clock.o $(HROBJ)/jlib/lib.j $(CRT) $(LIBM) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HROBJ)/clock/cmgr.o $(HROBJ)/clock/clock.o $(HROBJ)/jlib/lib.j $(LIBM) $(LIBC)
$(HRBIN)/clocksh: $(HROBJ)/clock/clocksh.o $(HROBJ)/jlib/lib.j $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HROBJ)/clock/clocksh.o $(HROBJ)/jlib/lib.j $(LIBC)

# --- the GUI launcher (hrconsole) --- loads /drv/hr, forks the five managers,
# waits for the desktop (dmgr) to exit, then unloads the driver.  Self-contained
# (spawn/waitc/panic are defined in hrconsole.c); links only libc.
$(HRBIN)/hrconsole: $(HROBJ)/misc/hrconsole.o $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HROBJ)/misc/hrconsole.o $(LIBC)

# --- fonts --- installed verbatim from _graphics/hr/fonts/ to /usr/hr/fonts/.
# smgr and dmgr require "sysfont" (the system font, == gacha.b.8) at startup.
HRFONTSRC = _graphics/hr/fonts
HRFONTS  := $(patsubst $(HRFONTSRC)/%,$(ROOT)/usr/hr/fonts/%,$(wildcard $(HRFONTSRC)/*))
$(ROOT)/usr/hr/fonts/%: $(HRFONTSRC)/%
	@mkdir -p $(dir $@)
	cp $< $@

# --- kernel driver (hr) --- loadable l.out, keep globals (-X) so `load` finds
# the hrcon_ config symbol; bind -k against the symboled kernel; must be +x.
# The driver carries NO font: hrgui loads the font file into the shared VRAM tail
# (src/userland/hr/inc/shmem.h) and every client blits from that single copy.
# NOTE: the rule that builds /drv/hr now lives in the ZView section below.  The
# driver source moved out of the untracked _graphics/ salvage into the tracked
# tree at $(HRGUISRC)/drv/; the legacy `hr' target still uses it, because there
# is only one hr driver and now only one rule for it.

HR_TARGETS := $(DRVDIR)/hr \
	$(addprefix $(HRBIN)/,smgr dmgr gmgr gsh clock clocksh hrconsole) \
	$(HRFONTS)
hr: $(HR_TARGETS)

# ZView - the rebuilt windowing system  (src/userland/hr -> build/root).
# Built as part of the normal `all'/`image' build (its outputs are in
# HRGUI_TARGETS, folded into `image' below) -- there is no separate target.
# GUI.md's green-field rebuild: the rendering engine is salvaged from the old
# _graphics/hr smgr into a standalone library libhrgfx.a (Phase 0), then a
# single window server + clock client are built on top (Phase 1), replacing the
# fragile jlib/coroutine/per-daemon IPC layer with one blocking-read server
# over pipe(2) + a shared-VRAM ring.  (The historical `hr' target above builds
# the original, buggy stack for reference.)
#
#   gfx/     libhrgfx.a  - the divorced rendering engine (asm blitters, bitblt,
#                          layer clipper, line/point/text/font rasterizers).
#                          globals.o holds the engine's global state and is
#                          linked DIRECTLY by each consumer (it defines only
#                          tentative/common symbols, which Coherent's one-pass
#                          ld will not pull from an archive).
#   gfx/gfxtest          - Phase 0 standalone draw test (links libhrgfx alone).
HRGUISRC = $(UL)/hr
HRGFXDIR = $(HRGUISRC)/gfx
HRGUIOBJ = $(OBJ)/userland/hr
HRGUIBIN = $(ROOT)/usr/hr/bin

# The engine's sources #include <smgr.h> etc. from their own directory.
# -Wa,-S: string literals into the SHARED data section (as -S rebinds .strn
# into L_SHRD) -- under an ld -n link (every z* client, zview) and in the
# shared library the literals then live in the read-only shared half, one
# copy per binary/library instead of one per process.  Requires that no hr
# source writes into a string literal (they don't; a violation faults
# loudly as SIGSEGV on first write).  Harmless in combined static links
# (gfxtest), where SHRD is writable.
HRGFXCFLAGS = -O -ftraditional -Dreadonly=const -I$(INCSRC) -I$(HRGFXDIR) -Wa,-S

# ZView headers: the engine's (gfx/) and the client/server contract (inc/), plus
# the system ones.  inc/ matters most -- wire.h and hrapp.h define records and
# structs that the server and EVERY client must agree on byte for byte, so a
# client object left unrebuilt against a changed wire.h is a live memory bug.
HRGUIHDRS := $(wildcard $(HRGUISRC)/gfx/*.h $(HRGUISRC)/inc/*.h) $(HDRS)

# ZView object pattern rules (own CFLAGS; the .s use cpp directives like the
# historical hr blitters, so preprocess with cpp -P before as).
$(HRGUIOBJ)/%.o: $(HRGUISRC)/%.c $(HRGUIHDRS)
	@mkdir -p $(dir $@)
	$(CC) $(HRGFXCFLAGS) -c $< -o $@
$(HRGUIOBJ)/%.o: $(HRGUISRC)/%.s $(HRGUIHDRS)
	@mkdir -p $(dir $@)
	$(CPP) -P -I$(HRGFXDIR) $< $@.i
	$(AS) $(ASFLAGS) -o $@ $@.i

# libhrgfx.a: the engine code (functions).  globals.o is deliberately excluded
# (linked directly by consumers, see above).  Member order is not load-bearing
# here, so use the plain kernel-style archive rule.
HRGFX_ASM := $(addprefix $(HRGUIOBJ)/gfx/,ablt.o small1.o small2.o block1.o block2.o ptrmath.o fcpy.o glftn.o)
HRGFX_C   := $(addprefix $(HRGUIOBJ)/gfx/,bitblt.o lblt.o layer.o masks.o rmath.o gcoord.o gline.o gpoint.o gtext.o gtext2.o f2.o gfxhooks.o)
HRGFX_GLOB := $(HRGUIOBJ)/gfx/globals.o
# libhrgfx.a is a build-time-only artifact (statically linked into the server
# and test); keep it in the obj tree so it is not packed into the disk image.
LIBHRGFX  := $(HRGUIOBJ)/libhrgfx.a

$(LIBHRGFX): $(HRGFX_ASM) $(HRGFX_C)
	$(ar-kernel)

# Phase 0 draw test: links libhrgfx + globals.o + libc only (no server, no IPC).
# That it links with no undefined message/jlib symbols IS the divorce gate.
$(HRGUIBIN)/gfxtest: $(HRGUIOBJ)/gfx/gfxtest.o $(HRGFX_GLOB) $(LIBHRGFX) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HRGUIOBJ)/gfx/gfxtest.o $(HRGFX_GLOB) $(LIBHRGFX) $(LIBC)

# --- Phase 1: window server + clock client ---
# Both need the shared wire protocol header in addition to the engine headers.
$(HRGUIOBJ)/zview/zview.o $(HRGUIOBJ)/zview/zvpump.o \
	$(HRGUIOBJ)/zview/zvwatch.o \
	$(HRGUIOBJ)/zclock/zclock.o $(HRGUIOBJ)/zdlg/zdlg.o \
	$(HRGUIOBJ)/zedit/zedit.o $(HRGUIOBJ)/zmail/zmail.o \
	$(HRGUIOBJ)/zprint/zprint.o $(HRGUIOBJ)/zcalc/zcalc.o \
	$(HRGUIOBJ)/zman/zman.o $(HRGUIOBJ)/zfile/zfile.o \
	$(HRGUIOBJ)/zpuzzle/zpuzzle.o $(HRGUIOBJ)/zdock/zdock.o \
	$(HRGUIOBJ)/zmaze/zmaze.o $(HRGUIOBJ)/zmaze/zmcore.o \
	$(HRGUIOBJ)/clgfx/clgfx.o $(HRGUIOBJ)/clgfx/hrlock.o \
	$(HRGUIOBJ)/clgfx/hrsel.o $(HRGUIOBJ)/cmd/hrclip.o \
	$(HRGUIOBJ)/clgfx/hrapp.o $(HRGUIOBJ)/clgfx/hrdlg.o \
	$(HRGUIOBJ)/clgfx/hrwl.o \
	$(HRGUIOBJ)/clgfx/hrsbar.o: HRGFXCFLAGS += -I$(HRGUISRC)/inc

# clgfx.o: the client-side direct-render draw library (GUI.md Model A).  Clients
# link it + globals.o + libhrgfx.a, which pulls ONLY bitblt + its asm inner loops
# + rmath + masks + gfxhooks + lblt.o (the client-safe blit/addressing half of
# the old layer.c) -- NOT layer.o, the server-only layering machinery.  That
# split is what keeps ~7K of restack/update code out of every client binary;
# if a client link starts pulling layer.o again, something in the client path
# grew a reference to a server-only symbol.
# hrlock.o + hrtas.o: the global drawing lock -- a userland TSET futex fast path
# (hrtas.s) with a kernel slow path (CIOMLOCK/CIOMUNLOCK) taken only on
# contention.  Linked into every client AND the server.
# hrapp.o: client start-up (hr_open) -- an app declares its own window (title,
# size, icon, flags) to the server instead of taking them on the command line.
HRTAS  := $(HRGUIOBJ)/clgfx/hrtas.o
HRLOCK := $(HRGUIOBJ)/clgfx/hrlock.o $(HRTAS)
# hrsel.o: the PRIMARY selection store (inc/shmem.h).  It needs the TSET but NOT
# hrlock.o -- the selection has its own lock word (SHM_SELLOCK) precisely so that
# a copy never takes the global drawing lock and stalls the UI.  Kept apart from
# HRLOCK so a consumer that only wants the selection (hrpump, hrclip) links
# $(HRSEL) $(HRTAS) and no drawing-lock code at all.
HRSEL := $(HRGUIOBJ)/clgfx/hrsel.o
# NOT in CLGFX: a client that never touches the selection (zclock) should not
# carry the store.  Consumers name $(HRSEL) explicitly.
# hrwl.o: the published-window-list reader (shmem.h SHM_WINLIST) -- pure
# seqlocked reads of the tail, no lock code at all.  Same rule as HRSEL:
# named explicitly by consumers that enumerate windows; exported to every
# shared client via the .sl below.  (The PUBLISHER is in zview itself.)
HRWL := $(HRGUIOBJ)/clgfx/hrwl.o
CLGFX := $(HRGUIOBJ)/clgfx/clgfx.o $(HRGUIOBJ)/clgfx/clrow.o \
	$(HRGUIOBJ)/clgfx/hrapp.o $(HRLOCK)
# hrdlg.o: the modal-dialog widget kit (inc/hrdlg.h).  Same rule as HRSEL --
# NOT in CLGFX; a client with no dialogs should not carry the widget code.
HRDLG := $(HRGUIOBJ)/clgfx/hrdlg.o
# hrsbar.o: the vertical-scrollbar common control (inc/hrsbar.h) -- a window-
# content sibling of the dialog widgets, shared by zterm and zedit.
# Same rule again: named explicitly by clients that scroll a view.
HRSBAR := $(HRGUIOBJ)/clgfx/hrsbar.o

# --- libhrgfx.sl: the shared gfx library (kernel LF_SLIB, slot 1) ---
# The whole client-side gfx stack -- engine minus the server-only layer.o,
# plus the clgfx layer, the selection store, the dialog kit, the scrollbar,
# and globals.o -- linked ONCE as an LF_SHR image whose text (+ -S string
# literals) sits at system segment 0x35 (mapped once for every process by
# the kernel at load; see exec.c/slmap) and whose private half (blitter
# templates, engine globals, commons) sits at segment 2, copied per process
# from the holder's pristine template at exec.  Clients name the .sl on
# their ld line: ld imports every global as an absolute address, so they
# link NO gfx objects at all.  The trailing DTOA + libc bake in the libc
# subset the library itself calls (clients bind those few symbols from the
# library too; the REAL dtoa goes in so a client's printf %f bound from the
# library formats floats -- zcalc).  NOT stripped: clients read the symbol
# table.  mkslib.py validates the halves fit their 64K segments, stamps
# LF_SLIB and sets l_entry to the slot's text base.
# NOT converted to .sl clients: zview + gfxtest (the server side keeps its
# own layer.o/globals statically), and hrpump/hrclip/zvpump/zvwatch -- for
# a 400-byte pump, attaching the ~10K per-process data template would be a
# regression, not a saving.
SHLIB := $(LIBDIR)/libhrgfx.sl
# slcrt.o: the library-side crt -- the absolutes crts0.s normally provides
# (SS, errno_) and the raw _exit stub, without crts0's start code (a library
# has no main).  The library's baked-in sbrk caches its break in its own
# seg-1 `end', which is exactly right: the kernel's ubrk grows the segment
# the break address lies in, so a shared client's heap lives in segment 1
# behind the library data (and a client's OWN heap, if it pulls its own
# malloc, stays in its own data segment -- the two coexist).
SLCRT := $(OBJ)/userland/lib/csu/slcrt.o
SLGFX_OBJ := $(HRGFX_ASM) $(filter-out $(HRGUIOBJ)/gfx/layer.o,$(HRGFX_C)) \
	$(HRGFX_GLOB) $(CLGFX) $(HRSEL) $(HRDLG) $(HRSBAR) $(HRWL)

$(SHLIB): $(SLCRT) $(SLGFX_OBJ) $(DTOA) $(LIBC) tools/mkslib.py
	@mkdir -p $(dir $@)
	$(LD) -n -X -R 0x35000000 -D 0x2000000 -o $@ $(SLCRT) $(SLGFX_OBJ) $(DTOA) $(LIBC)
	$(PYTHON) tools/mkslib.py --slot 1 $@

# --- libc.sl: the shared C library (kernel LF_SLIB, slot 0) ---
# All of libc as one LF_SHR image: text+rodata at system segment 0x34
# (mapped once for every process), data at hardware segment 1 (pristine
# per-process template).  Excluded, and supplied by the trailing static
# libc.a on every link line instead:
#   sdtoa        - the "No floating point!" stub is meaningless in a shared
#                  image (the one copy serves everyone, so the real dtefg
#                  is always in -- shared clients get real %f for free);
#                  the archive-order trick lives on in the static libc.a.
#   _prof/monitor- profiled builds fall back to fully static links.
#   _finish      - the no-stdio stub twin of finit.o's real flusher (the
#                  same archive-order pair as sdtoa/dtefg): the shared
#                  image always carries real stdio, so the real one wins.
#   crypt getgrent getpwent getwd getpass - data-heavy leaf modules
#                  (~3K of per-process buffers/tables) used by few
#                  programs; excluding them shrinks the flat per-process
#                  data copy every shared command pays.
#   notify       - references the excluded getpwuid; its few users (mail)
#                  pull it and getpwent together from the static libc.a.
SLIBC_OMIT := gen/sdtoa.o crt/_prof.o gen/monitor.o gen/_finish.o \
	gen/notify.o gen/crypt.o \
	gen/getgrent.o gen/getpwent.o gen/getwd.o gen/getpass.o
SLIBC_OBJ := $(SLCRT) \
	$(filter-out $(addprefix $(OBJ)/userland/lib/libc/,$(SLIBC_OMIT)),$(LIBC_OBJ))

$(LIBC_SL): $(SLIBC_OBJ) tools/mkslib.py
	@mkdir -p $(dir $@)
	$(LD) -n -X -R 0x34000000 -D 0x1000000 -o $@ $(SLIBC_OBJ)
	$(PYTHON) tools/mkslib.py --slot 0 $@

# zview owns the screen: links the engine (libhrgfx) + globals.o directly.
# Fonts are loaded at runtime from /usr/hr/fonts/*.hf into the shared VRAM tail
# (inc/shmem.h) and blitted with the engine's bitblt -- no embedded/kernel font.
# Linked -n: zview fork()s for every app launch (launchapp), driver loads, the
# rc and the watchdog -- shared text turns each ~55K transient text copy into a
# refcount bump, flattening the RAM spike at exactly the moment apps start.
$(HRGUIBIN)/zview: LDNFLAGS := -n
$(HRGUIBIN)/zview: $(HRGUIOBJ)/zview/zview.o $(HRLOCK) $(HRSEL) $(HRGFX_GLOB) $(LIBHRGFX) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zview/zview.o $(HRLOCK) $(HRSEL) $(HRGFX_GLOB) $(LIBHRGFX) $(LIBC)

# zvpump / zvwatch: zview's input pump and crash watchdog as TINY libc-only
# programs, exec'd over what would otherwise be full ~69Kb fork copies of the
# non-shared server image held for the whole session (same cure as zterm's
# hrpump; see zview.c startpump/srvwatch).
$(HRGUIBIN)/zvpump: $(HRGUIOBJ)/zview/zvpump.o $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HRGUIOBJ)/zview/zvpump.o $(LIBC)

$(HRGUIBIN)/zvwatch: $(HRGUIOBJ)/zview/zvwatch.o $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HRGUIOBJ)/zview/zvwatch.o $(LIBC)

# zclock: direct-render graphics client -- draws via the shared library (its
# face/hands blit straight to VRAM); needs libm (sin/cos).
# -n costs nothing and shares text if a second clock is ever opened.
$(HRGUIBIN)/zclock: LDNFLAGS := -n
$(HRGUIBIN)/zclock: $(HRGUIOBJ)/zclock/zclock.o $(SHLIB) $(CRT) $(LIBM) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zclock/zclock.o $(SHLIB) $(LIBM) $(LIBC)

# zdlg: the dialog demo / test client (widget kit via the shared library).
$(HRGUIBIN)/zdlg: LDNFLAGS := -n
$(HRGUIBIN)/zdlg: $(HRGUIOBJ)/zdlg/zdlg.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zdlg/zdlg.o $(SHLIB) $(LIBC)

# ptytest is a plain client: no gfx, just exercises the kernel pty driver.
$(HRGUIBIN)/ptytest: $(HRGUIOBJ)/ptytest/ptytest.o $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HRGUIOBJ)/ptytest/ptytest.o $(LIBC)

# zterm: direct-render terminal -- pty + VT parser + clgfx (blits its own text
# straight to VRAM, hardware-scrolls when fully visible).
# Linked -n (shared text, via LDNFLAGS): N open terminals share ONE in-core
# copy of the ~26K text.  This requires every runtime-written engine buffer to
# be in the private sections -- see block1.s code_space_/sreg_ (.prvi/.prvd);
# the shared segment [SHRI+SHRD] is mapped read-only and reused across
# instances.
$(HRGUIOBJ)/zterm/zterm.o: HRGFXCFLAGS += -I$(HRGUISRC)/inc
$(HRGUIBIN)/zterm: LDNFLAGS := -n
$(HRGUIBIN)/zterm: $(HRGUIOBJ)/zterm/zterm.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zterm/zterm.o $(SHLIB) $(LIBC)

# zedit: direct-render text editor -- the diff renderer + scrollbar of zterm,
# the dialog kit for its Open/Save file dialogs, and the selection store for
# Cut/Copy/Paste.  -n: shared text across editor instances, like zterm.
$(HRGUIBIN)/zedit: LDNFLAGS := -n
$(HRGUIBIN)/zedit: $(HRGUIOBJ)/zedit/zedit.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zedit/zedit.o $(SHLIB) $(LIBC)

# zmail: direct-render mail client -- zedit's diff renderer + scrollbars over
# the 7mail spool format (/usr/spool/mail/<user>); the dialog kit for its
# notices and the selection store for middle-click paste while composing.
$(HRGUIBIN)/zmail: LDNFLAGS := -n
$(HRGUIBIN)/zmail: $(HRGUIOBJ)/zmail/zmail.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zmail/zmail.o $(SHLIB) $(LIBC)

# zmon: direct-render system monitor -- a memory bar over a process list.
# The data comes the way ps and mem get it (nlist /coherent, walk procq and
# segmq through a /dev/kmem arena snapshot, command lines via /dev/mem and
# /dev/swap), so like those two it compiles against the machine-specific
# kernel headers.  Read-only: no dialogs, no selection store.
$(HRGUIOBJ)/zmon/zmon.o: HRGFXCFLAGS += -I$(HRGUISRC)/inc \
	-I$(SRC)/kernel/z8001/h -I$(SRC)/kernel/h
$(HRGUIBIN)/zmon: LDNFLAGS := -n
$(HRGUIBIN)/zmon: $(HRGUIOBJ)/zmon/zmon.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zmon/zmon.o $(SHLIB) $(LIBC)

# zprint: direct-render print manager -- zmail's skeleton (button bar + job
# list + detail pane) over the lpr spool (/usr/spool/lpd cf files); the dialog
# kit for its Print.../confirm dialogs.  No selection store: nothing to paste.
$(HRGUIBIN)/zprint: LDNFLAGS := -n
$(HRGUIBIN)/zprint: $(HRGUIOBJ)/zprint/zprint.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zprint/zprint.o $(SHLIB) $(LIBC)

# zcalc: direct-render desk calculator -- a fixed window of chrome buttons.
# Double-precision arithmetic shown with %g, so like factor/units it links the
# real dtoa formatter ahead of libc; the scientific pad pulls libm.
$(HRGUIBIN)/zcalc: LDNFLAGS := -n
$(HRGUIBIN)/zcalc: $(HRGUIOBJ)/zcalc/zcalc.o $(SHLIB) $(CRT) $(DTOA) $(LIBM) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(DTOA) $(HRGUIOBJ)/zcalc/zcalc.o $(SHLIB) $(LIBM) $(LIBC)

# zman: direct-render manual-page browser -- zprint's skeleton (find field +
# list + content pane) over the pre-formatted catman pages in /usr/man, with
# the nroff overstrikes rendered as real bold (transparent double-strike,
# cl_ptextt) and underline.  No dialogs: the find field lives in the bar
# (hrdlg.h is included for the DLG_* metrics only).  HRSEL: the content pane
# is select-to-copy, so it writes the PRIMARY selection store like zterm.
$(HRGUIBIN)/zman: LDNFLAGS := -n
$(HRGUIBIN)/zman: $(HRGUIOBJ)/zman/zman.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zman/zman.o $(SHLIB) $(LIBC)

# zpuzzle: the 15 puzzle (the early X demos' `puzzle') -- a fixed board of
# sliding tiles, clgfx only: no scrollbar, no dialogs.
$(HRGUIBIN)/zpuzzle: LDNFLAGS := -n
$(HRGUIBIN)/zpuzzle: $(HRGUIOBJ)/zpuzzle/zpuzzle.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zpuzzle/zpuzzle.o $(SHLIB) $(LIBC)

# zmaze: Wolfenstein-style raycast maze in a fixed 320x200 window.  The
# render core (zmcore.c) never divides -- tables + the zmaze_a.s asm inner
# loops (DDA, edge compositor, dither fills; r0-r5 scratch ABI) -- and the
# frame is presented with cl_blit (ldir rows when frontmost + word-aligned).
$(HRGUIBIN)/zmaze: LDNFLAGS := -n
$(HRGUIBIN)/zmaze: $(HRGUIOBJ)/zmaze/zmaze.o $(HRGUIOBJ)/zmaze/zmcore.o \
		$(HRGUIOBJ)/zmaze/zmaze_a.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zmaze/zmaze.o \
		$(HRGUIOBJ)/zmaze/zmcore.o $(HRGUIOBJ)/zmaze/zmaze_a.o \
		$(SHLIB) $(LIBC)

# zdock: the dock -- the desktop's separate "shell" (icon bar along the top,
# app launching/switching).  An ordinary client with an UNDECORATED window
# (HRF_NODECOR), started from the desktop rc; the server draws no icons of
# its own any more.  clgfx + the window-list reader via the shared library.
$(HRGUIBIN)/zdock: LDNFLAGS := -n
$(HRGUIBIN)/zdock: $(HRGUIOBJ)/zdock/zdock.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zdock/zdock.o $(SHLIB) $(LIBC)

# zfile: direct-render file manager -- an ls -l listing of one directory with
# open/copy/move/delete/mkdir through the dialog kit.  Launched executables
# inherit the command pipe, so any +x binary becomes a launchable GUI app.
$(HRGUIBIN)/zfile: LDNFLAGS := -n
$(HRGUIBIN)/zfile: $(HRGUIOBJ)/zfile/zfile.o $(SHLIB) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zfile/zfile.o $(SHLIB) $(LIBC)

# hrpump: the terminal's I/O pumps, a tiny libc-only helper zterm execs instead
# of forking copies of itself (memory: keeps a few open terminals from exhausting
# RAM).  NO libhrgfx/clgfx -- it only shuffles bytes between fds.
$(HRGUIOBJ)/zterm/hrpump.o: HRGFXCFLAGS += -I$(HRGUISRC)/inc
$(HRGUIBIN)/hrpump: LDNFLAGS := -n
$(HRGUIBIN)/hrpump: $(HRGUIOBJ)/zterm/hrpump.o $(HRSEL) $(HRLOCK) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s $(LDNFLAGS) -o $@ $(CRT) $(HRGUIOBJ)/zterm/hrpump.o $(HRSEL) $(HRLOCK) $(LIBC)

# hrclip: read/set the PRIMARY selection from a shell.  Not a GUI client -- no
# window, no clgfx, no engine; just the selection store, whose writer side needs
# hrtas.o for the TSET.  It is also how the store is tested on the plain serial
# emulator, before any mouse plumbing exists.
$(HRGUIBIN)/hrclip: $(HRGUIOBJ)/cmd/hrclip.o $(HRSEL) $(HRLOCK) $(CRT) $(LIBC)
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(HRGUIOBJ)/cmd/hrclip.o $(HRSEL) $(HRLOCK) $(LIBC)

# The ZView system fonts: generated into the shared-VRAM .hf format the server
# loads into the tail (tools/mkfont.py); every client blits from that single
# copy -- no relink, no per-glyph trap.  All three come from the FM fonts:
# gacha.r.hf 8x15 (terminal -- 8px cell = byte-aligned glyph output),
# gacha.b.hf 9x16 (UI chrome) and sail.hf 6x8 (icon labels).
HRGUIFONTS = $(ROOT)/usr/hr/fonts/gacha.r.hf $(ROOT)/usr/hr/fonts/gacha.b.hf \
	$(ROOT)/usr/hr/fonts/sail.hf
$(HRGUIFONTS): tools/mkfont.py $(ROOT)/usr/hr/fonts/gacha.r.7 \
		$(ROOT)/usr/hr/fonts/gacha.b.8 $(ROOT)/usr/hr/fonts/sail.r.6
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mkfont.py

# the launchable-app catalog zview reads at startup (src -> staging image).
$(ROOT)/usr/hr/etc/apps: src/userland/hr/etc/apps
	@mkdir -p $(dir $@)
	cp $< $@

# the desktop start-up script: a /bin/sh script zview runs when it comes up,
# naming the apps to open (with their -P/-S/-H options) -- so the first screen
# is configuration, not server code (src -> staging image).
$(ROOT)/usr/hr/etc/rc: src/userland/hr/etc/rc
	@mkdir -p $(dir $@)
	cp $< $@

# desktop icons (.icn) the dock (zdock) blits in its bar (src -> staging image).
HRGUIICONS := $(patsubst src/userland/hr/icons/%,$(ROOT)/usr/hr/icons/%,\
	$(wildcard src/userland/hr/icons/*.icn))
$(ROOT)/usr/hr/icons/%: src/userland/hr/icons/%
	@mkdir -p $(dir $@)
	cp $< $@

# --- the hi-res driver (/drv/hr) ---
# Keyboard ISR, polled mouse, hardware cursor, and the drawing-lock futex slow
# path (CIOMLOCK/CIOMUNLOCK).  Loadable l.out: keep globals (-X) so `load' finds
# the hrcon_ config symbol, bind -k against the symboled kernel, and it must be +x.
#
# It is a KERNEL translation unit but it lives under the ZView tree because it
# belongs to this subsystem rather than to the generic kernel -- it is versioned
# and changed together with the server and clients that depend on it, and it was
# the last piece still being built out of the untracked _graphics/ salvage.
# hr.c #includes hr2.c, so the two are a single object.  Kernel CFLAGS, with the
# engine's headers on the path for rico.h.
$(HRGUIOBJ)/drv/hr.o: HRGFXCFLAGS = $(KCFLAGS) -I$(HRGFXDIR)
$(HRGUIOBJ)/drv/hr.o: $(HRGUISRC)/drv/hr2.c $(HRGUISRC)/drv/hr.h $(KHDRS)

$(DRVDIR)/hr: $(HRGUIOBJ)/drv/hr.o $(HRGUIOBJ)/drv/hrasm.o $(KSYM)
	@mkdir -p $(dir $@)
	$(LD) -X -o $@ $(HRGUIOBJ)/drv/hr.o $(HRGUIOBJ)/drv/hrasm.o -k$(KSYM)
	chmod +x $@

HRGUI_TARGETS := $(DRVDIR)/hr $(LIBHRGFX) $(SHLIB) $(HRGUIBIN)/gfxtest $(HRGUIBIN)/zview \
	$(HRGUIBIN)/zvpump $(HRGUIBIN)/zvwatch $(HRGUIBIN)/zclock \
	$(HRGUIBIN)/zdlg $(HRGUIBIN)/zedit $(HRGUIBIN)/zmail $(HRGUIBIN)/zprint \
	$(HRGUIBIN)/zmon $(HRGUIBIN)/zcalc $(HRGUIBIN)/zman $(HRGUIBIN)/zfile \
	$(HRGUIBIN)/zpuzzle $(HRGUIBIN)/zmaze $(HRGUIBIN)/zdock \
	$(HRGUIBIN)/ptytest $(HRGUIBIN)/zterm $(HRGUIBIN)/hrpump $(HRGUIBIN)/hrclip \
	$(HRGUIFONTS) \
	$(ROOT)/usr/hr/etc/apps $(ROOT)/usr/hr/etc/rc $(HRGUIICONS)

# Build the ZView desktop and its clients standalone (they are otherwise only
# reachable through `image`, which repacks the whole disk).
hrgui: $(HRGUI_TARGETS)

# ===========================================================================
# Prebuilt / script artifacts  (src/dist -> build/root)
# ===========================================================================
# Files installed verbatim into the staging image because they have no
# rebuildable source in this tree.  src/dist mirrors the target filesystem
# layout, so each file drops straight into build/root/:
#   bin/       cc,ccx wrappers + source-less binaries (db,nld), man script,
#              true/false
#   etc/       cpsys script, boottime marker
#   lib/       prebuilt C toolchain passes cpp,cc0..cc3 + yacc skeleton
#              yyparse.c, and the small-model runtime scrts0.o / slibc.a (no
#              scrts0.s source and no small-model build wiring exist yet)
#   etc/       system config/data: .profile,profile,group,passwd,rc,ttys,motd,
#              news,newusr,logmsg,helpfile,helpindex,termcap, plus runtime-state
#              seeds mtab,mnttab,utmp,ddate (see caveat below)
#   usr/games/ rubik script + lib/ data (fortunes,rubik.m4); fortune,moo are
#              built from src/userland/cmd (decompiled from disasm/*.asm), as
#              are /bin/l and /etc/fdformat
#   usr/pub/   ascii chart
#   usr/lib/   data/config with no compile step: units/binunits databases,
#              lib.b (m4), crontab, make{macros,actions}, tmac.an/tmac.s (nroff)
# NOT here, on purpose:
#   * the kernel image /coherent, the swapper /etc/swap and the video drivers
#     /drv/lrtty (low-res) and /drv/hrtty (hi-res) are built by the `kernel`
#     target; the compiler passes' large-model libc/crt come from `libs`, and
#     /usr/lib atrun/diff3/diffh/lpd/spell + libmp.a from `cmds`.
#   * /etc/proto/proto.hd{1,2,3} are generated by mkproto at install time.
#   * mtab/mnttab/utmp/ddate are live runtime state; they are staged verbatim to
#     match the reference image, but a truly clean install should zero them.
#
# The tree is shallow; glob a few levels (incl. dotfiles like .profile) and drop
# directory entries so `cp` only ever sees regular files.  A recursive
# $(shell find ...) is avoided because on a typical Windows PATH `find` is
# System32\find.exe, not coreutils; the `.*` globs pull in dotfiles and the
# directory filter also discards the resulting `.`/`..` entries.
dist_all     := $(wildcard $(SRC)/dist/* $(SRC)/dist/*/* $(SRC)/dist/*/*/* \
                $(SRC)/dist/*/*/*/* $(SRC)/dist/*/.* $(SRC)/dist/*/*/.*)
dist_dirs    := $(patsubst %/.,%,$(wildcard $(addsuffix /.,$(dist_all))))
DIST_SRC     := $(filter-out $(dist_dirs),$(dist_all))
DIST_TARGETS := $(patsubst $(SRC)/dist/%,$(ROOT)/%,$(DIST_SRC))

$(DIST_TARGETS): $(ROOT)/%: $(SRC)/dist/%
	@mkdir -p $(dir $@)
	cp $< $@

dist: $(DIST_TARGETS)

# ===========================================================================
# Manual pages  (src/userland/man -> pre-formatted catman pages in /usr/man)
# ===========================================================================
# Each nroff -man source under src/userland/man/<N>cmd/ is formatted at build
# time with the HOST nroff (coherent-nroff) into a pre-formatted "catman" page
# under build/root/usr/man/<N>cmdman/.  This release's /bin/man is a shell
# script that, by default, pages a pre-formatted file it finds by globbing
# /usr/man/*cmdman (and *sysman) with `scat -s`; our 1cmd/8cmd sections install
# as 1cmdman/8cmdman so both match the *cmdman glob.  (`man -n` would re-nroff a
# source under /usr/man/*cmd, which we do not install.)
#
# -x drops the trailing page pad, so a page is ~1KB.  COHERENT_TMAC points the
# host nroff at the installed tmac.an (the -man macro package); it is given as a
# RELATIVE path so its Windows drive-letter form never matters.  The pages must
# also be listed in src/image/hdd_manifest.txt (644 0 1) or `make image` errors
# on the undeclared paths -- the manifest is authoritative for what ships.
MANROFF  ?= coherent-nroff
MAN_TMAC  := $(ROOT)/usr/lib/tmac.an
MAN1_SRC  := $(wildcard $(UL)/man/1cmd/*)
MAN8_SRC  := $(wildcard $(UL)/man/8cmd/*)
MAN_TARGETS := $(patsubst $(UL)/man/1cmd/%,$(ROOT)/usr/man/1cmdman/%,$(MAN1_SRC)) \
               $(patsubst $(UL)/man/8cmd/%,$(ROOT)/usr/man/8cmdman/%,$(MAN8_SRC))

$(ROOT)/usr/man/1cmdman/%: $(UL)/man/1cmd/% $(MAN_TMAC)
	@mkdir -p $(dir $@)
	COHERENT_TMAC="$(ROOT)/usr/lib" $(MANROFF) -x -man $< > $@ || { rm -f $@; exit 1; }
$(ROOT)/usr/man/8cmdman/%: $(UL)/man/8cmd/% $(MAN_TMAC)
	@mkdir -p $(dir $@)
	COHERENT_TMAC="$(ROOT)/usr/lib" $(MANROFF) -x -man $< > $@ || { rm -f $@; exit 1; }

man: $(MAN_TARGETS)

# ===========================================================================
# Disk image  (pack build/root -> build/dist/hdd.bin)
# ===========================================================================
# tools/build_disk.py takes file CONTENTS from build/root and applies ownership,
# permissions and the /dev device nodes from the manifests.  A Windows staging
# tree cannot carry uid/gid, setuid bits, or device special files, so these are
# applied at pack time (see CLAUDE.md).  The four partitions are formatted with
# the C900 per-partition geometry hardcoded in build_disk.py; hd0 is populated
# from build/root, hd1-hd3 are left as empty filesystems.
#
# The manifests are checked-in, hand-maintained build inputs in src/image/: a
# new file has to be added there as well as to build/root.
PYTHON ?= python
DISKIMG      := $(DISTDIR)/hdd.bin
HDD_MANIFEST := $(SRC)/image/hdd_manifest.txt
HDD_DEVICES  := $(SRC)/image/hdd_devices.txt

image: headers libs cmds kernel dist man hr $(HRGUI_TARGETS)
	$(PYTHON) tools/build_disk.py --root "$(ROOT)" \
	    --perms "$(HDD_MANIFEST)" --devices "$(HDD_DEVICES)" --out "$(DISKIMG)"

# ===========================================================================
# Floppy image  (minimal bootable system -> build/dist/floppy.img)
# ===========================================================================
# A single-partition C900 floppy (geometry from the reference boot floppy).  The
# floppy kernel (fdcon) is built to $(FLOPPYDIR)/coherent by the `kernel` target;
# `make floppy` (NOT part of `all`) stages a minimal system into build/floppy and
# packs it.  Ownership/permissions come from src/image/floppy_manifest.txt and
# the /dev nodes from src/image/floppy_devices.txt (currently the same set as the
# hard disk).
#
# The manifest is the SINGLE source of truth for what ships: stage every 'f'
# entry.  To add a command (e.g. /bin/ls) you edit ONLY floppy_manifest.txt.
# File CONTENT comes from build/root at the same path, EXCEPT: /coherent is the
# fdcon kernel built in place, and any file placed in the src/floppy/ overlay
# tree (mirroring the target path, e.g. src/floppy/etc/rc) overrides build/root.
FLOPPYIMG       := $(DISTDIR)/floppy.img
FLOPPY_MANIFEST := $(SRC)/image/floppy_manifest.txt
FLOPPY_DEVICES  := $(SRC)/image/floppy_devices.txt
FLOPPY_FILES := $(shell awk -F'\t' '$$1=="f"{print $$2}' $(FLOPPY_MANIFEST))
FLOPPY_RELS  := $(patsubst /%,%,$(filter-out /coherent,$(FLOPPY_FILES)))
# Content source for a floppy file: an overlay under src/floppy/ (mirroring the
# target path, e.g. src/floppy/etc/rc) wins over the build/root copy.
floppy_src = $(if $(wildcard $(SRC)/floppy/$(1)),$(SRC)/floppy/$(1),$(ROOT)/$(1))
FLOPPY_SRCS := $(foreach r,$(FLOPPY_RELS),$(call floppy_src,$(r)))

# `make floppy` first builds/collects the sources it needs (pulling in any
# commands that must be built), then rebuilds build/floppy FROM SCRATCH so no
# file dropped from the manifest can linger, and packs.  The wipe removes
# everything under build/floppy EXCEPT the in-place fdcon kernel (/coherent,
# built by `kernel`) -- "all but coherent" rather than a fixed directory list,
# so any directory layout in the manifest is handled automatically.
floppy: $(FLOPPYDIR)/coherent $(FLOPPY_SRCS)
	@for x in $(FLOPPYDIR)/*; do \
	    [ "$$x" = "$(FLOPPYDIR)/coherent" ] || rm -rf "$$x"; \
	done
	@for r in $(FLOPPY_RELS); do \
	    s=$(SRC)/floppy/$$r; [ -f "$$s" ] || s=$(ROOT)/$$r; \
	    d=$(FLOPPYDIR)/$$r; mkdir -p "$${d%/*}"; cp "$$s" "$$d"; \
	done
	$(PYTHON) tools/build_disk.py --floppy --root "$(FLOPPYDIR)" \
	    --perms "$(FLOPPY_MANIFEST)" --devices "$(FLOPPY_DEVICES)" --out "$(FLOPPYIMG)"

clean:
	rm -rf $(OBJ) $(LIBS) $(CMD_TARGETS) $(KERNEL_TARGETS) $(HR_TARGETS) $(HRGUI_TARGETS) $(DIST_TARGETS) \
	       $(ROOT)/usr/man \
	       $(DISKIMG) $(FLOPPYIMG) $(FLOPPYDIR)
