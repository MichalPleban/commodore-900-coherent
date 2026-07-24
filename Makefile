# Makefile - rebuild the Coherent Z8001 libraries and installed headers.
#
# Builds into the build/ staging tree (see CLAUDE.md):
#   build/root/usr/include/  - installed system headers
#   build/root/lib/          - crts0.o dtoa.o libc.a libm.a libmp.a liby.a
#   build/obj/               - intermediate .o/.s, mirroring src/
#
# Run with GNU make (recipes use an sh-compatible shell + coreutils).

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

CMDS = $(UL)/cmd

# ---------------------------------------------------------------------------
# Installed headers
# ---------------------------------------------------------------------------
HDRS       := $(wildcard $(INCSRC)/*.h $(INCSRC)/sys/*.h)
INC_TARGET := $(patsubst $(INCSRC)/%,$(INCDIR)/%,$(HDRS))

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
LIBM  = $(LIBDIR)/libm.a
# the reference image keeps libmp.a in /usr/lib, not /lib
LIBMP = $(USRLIBDIR)/libmp.a
LIBY  = $(LIBDIR)/liby.a
# libfs: shared file-system access for the check tools (icheck/dcheck/ncheck).
LIBFS = $(LIBDIR)/libfs.a

LIBS = $(CRT) $(DTOA) $(LIBC) $(LIBM) $(LIBMP) $(LIBY) $(LIBFS)

.PHONY: all headers libs cmds kernel dist man image diskmanifest floppy clean
all: headers libs cmds kernel dist man image
headers: $(INC_TARGET)
libs: $(LIBS)

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------
$(INCDIR)/%.h: $(INCSRC)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

# Generated sources (e.g. a yacc y.tab.c) live in the obj tree, not src/.
$(OBJ)/%.o: $(OBJ)/%.c
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
define link
	@mkdir -p $(dir $@)
	$(LD) -s -o $@ $(CRT) $(1) $(LIBC)
endef

# --- single-file commands -> /bin ------------------------------------------
BIN_CMDS := ac ar at bad banner basename c cal cat check chgrp chmod chown \
	cmp col comm conv cp cpdir crypt date dd deroff df diff3 du echo file \
	from head help join kill lc learn ln login look ls m4 mesg mkdir \
	msg mv newgrp nm od passwd pr prep prof pwd quot ranlib rev rm \
	rmdir sa scat size sleep sort split strip stty su sum sync tail tar tee \
	time touch tr tty typo uniq version wc who write yes

# icheck/dcheck/ncheck are built separately (they link libfs.a) -- see below.
FS_CMDS := icheck dcheck ncheck

# --- single-file commands -> /etc ------------------------------------------
ETC_CMDS := accton clri cron getty init mkfs mknod mkproto reboot umount update wall

# system utilities that pull in kernel headers (struct proc, drvcon.h, mount.h).
KERN_ETC_CMDS := load mount uload

BIN_TARGETS := $(addprefix $(BINDIR)/,$(BIN_CMDS))
ETC_TARGETS := $(addprefix $(ETCDIR)/,$(ETC_CMDS))
KERN_ETC_TARGETS := $(addprefix $(ETCDIR)/,$(KERN_ETC_CMDS))

$(BIN_TARGETS): $(BINDIR)/%: $(OBJ)/userland/cmd/%.o $(CRT) $(LIBC)
	$(call link,$<)

$(ETC_TARGETS): $(ETCDIR)/%: $(OBJ)/userland/cmd/%.o $(CRT) $(LIBC)
	$(call link,$<)

# init embeds VERSION from <machine.h>, so rebuild it when that header changes
# (the generic %.o rule has no header prerequisites).
$(OBJ)/userland/cmd/init.o: $(INCSRC)/machine.h

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

# sh: uses the checked-in y.tab.c; VERSION is normally `version` output.
sh_obj := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(wildcard $(CMDS)/sh/*.c))
$(sh_obj): CFLAGS += -I$(CMDS)/sh -DVERSION='"COHERENT"'
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
CMD_TARGETS := $(BIN_TARGETS) $(ETC_TARGETS) $(KERN_ETC_TARGETS) \
	$(addprefix $(BINDIR)/,$(FS_CMDS)) \
	$(BINDIR)/factor $(BINDIR)/units $(BINDIR)/mail $(USRLIBDIR)/atrun $(USRLIBDIR)/diff3 \
	$(BINDIR)/as $(BINDIR)/awk $(BINDIR)/bc $(BINDIR)/cu $(BINDIR)/dc \
	$(BINDIR)/expr $(BINDIR)/find $(BINDIR)/grep $(BINDIR)/ps \
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
# libcmdr.a: Commodore machine-dependent glue + al/lp drivers.
CMDR_OBJS := $(addprefix $(OBJ)/kernel/z8001/,\
	drv/al.o drv/lp.o src/commodore.o src/console.o src/ddt.o src/trap.o)

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

# Kernel C objects use the kernel flags (replacing the userland CFLAGS).
$(COH_OBJS) $(CMDR_OBJS) $(KWD) $(KSWAP) $(WDCON) $(FDCON) $(NOTTY_OBJS): CFLAGS = $(KCFLAGS)
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

# Shipped kernels: fully stripped (-s omits the whole symbol table).  Addresses
# match $(KSYM) exactly, so the loadables stay valid; this sheds the ~10.5K
# global symbol table from each shipped image.
$(ROOT)/coherent: $(KMD) $(WDCON) $(KWD) $(LIBCMDR) $(LIBCOH) $(LIBC)
	$(call link-kernel,-s,$(WDCON))
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

KERNEL_TARGETS := $(ROOT)/coherent $(FLOPPYDIR)/coherent $(ROOT)/etc/swap \
	$(DRVDIR)/lrtty $(DRVDIR)/hrtty $(DRVDIR)/notty
kernel: $(KERNEL_TARGETS)

# ===========================================================================
# Prebuilt / script artifacts  (src/dist -> build/root)
# ===========================================================================
# Files installed verbatim into the staging image because they have no
# rebuildable source in this tree.  src/dist mirrors the target filesystem
# layout, so each file drops straight into build/root/:
#   bin/       cc,ccx wrappers + source-less binaries (db,nld,l), man script,
#              true/false
#   etc/       cpsys script, fdformat binary, boottime marker
#   lib/       prebuilt C toolchain passes cpp,cc0..cc3 + yacc skeleton
#              yyparse.c, and the small-model runtime scrts0.o / slibc.a (no
#              scrts0.s source and no small-model build wiring exist yet)
#   etc/       system config/data: .profile,profile,group,passwd,rc,ttys,motd,
#              news,newusr,logmsg,helpfile,helpindex,termcap, plus runtime-state
#              seeds mtab,mnttab,utmp,ddate (see caveat below)
#   usr/games/ fortune,moo binaries + rubik script + lib/ data (fortunes,rubik.m4)
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
# the master's exact per-partition geometry; hd0 is populated from build/root,
# hd1-hd3 are left as empty filesystems.
#
# All manifests are checked-in, reviewed build inputs in src/image/ -- NOT
# re-extracted from the master on every build.  Regenerate the HD pair with
# `make diskmanifest` when the reference master changes, then review/commit.
PYTHON ?= python
MASTER ?= ../Emulator/disk/hdd.bin
DISKIMG      := $(DISTDIR)/hdd.bin
HDD_MANIFEST := $(SRC)/image/hdd_manifest.txt
HDD_DEVICES  := $(SRC)/image/hdd_devices.txt

image: headers libs cmds kernel dist man
	$(PYTHON) tools/build_disk.py build --root "$(ROOT)" \
	    --perms "$(HDD_MANIFEST)" --devices "$(HDD_DEVICES)" --out "$(DISKIMG)"

diskmanifest:
	$(PYTHON) tools/build_disk.py extract --master "$(MASTER)" \
	    --perms "$(HDD_MANIFEST)" --devices "$(HDD_DEVICES)"

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
	$(PYTHON) tools/build_disk.py build --floppy --root "$(FLOPPYDIR)" \
	    --perms "$(FLOPPY_MANIFEST)" --devices "$(FLOPPY_DEVICES)" --out "$(FLOPPYIMG)"

clean:
	rm -rf $(OBJ) $(LIBS) $(CMD_TARGETS) $(KERNEL_TARGETS) $(DIST_TARGETS) \
	       $(ROOT)/usr/man \
	       $(DISKIMG) $(FLOPPYIMG) $(FLOPPYDIR)
