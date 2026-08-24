# project.mk - the vellum sheet-set Makefile skeleton (VELLUM.md sec. 40).
# A new design starts with a copy, the way a new title block already does:
#	cp /usr/vellum/etc/project.mk Makefile
# then set SHEETS.  check: and drift: gate print: and ps: the way cc gates
# on -Werror -- no paper moves until the set proves out and the changes
# since the approved revision have been looked at.
#
# V names the EXPORTER, not the editor.  `vellum -mode' hands its
# arguments straight to this program anyway (main() execs it for any
# lowercase -mode), and the editor is a hi-res GRAPHICS client: on a
# machine with no bitmap card it cannot even exec, because the shared
# graphics library it links is not loaded there.  The exporter is the
# headless half and runs anywhere, which is what a Makefile wants.
# Say V=/usr/vellum/bin/vellum if you would rather type the front door
# and you know the machine has a screen.

V=/usr/vellum/lib/velxport
SHEETS=amp1.d amp2.d
REV=A
SPICE=deck.cir

check: ; $(V) -check $(SHEETS)

print: check drift ; $(V) -print $(SHEETS) | lpr

ps: check drift ; $(V) -ps -fit $(SHEETS) | lpr

plot: check ; $(V) -hpgl $(SHEETS)

bom: check ; $(V) -bom $(SHEETS)

net: check ; $(V) -net $(SHEETS)

# the takeoff: run lengths per net and per layer, cut-out areas, a total
len: ; $(V) -len $(SHEETS)

# the deck the department simulator reads; wrap it in a file that
# .INCLUDEs this one beside your .MODEL lines
spice: check ; $(V) -spice $(SHEETS) > $(SPICE)

# the drawing set's own -Werror for prose: fails while a sheet still
# carries a TODO note
todo: ; $(V) -where TODO $(SHEETS)

# REVISIONS are files and the record is the directory (sec. 50): no
# SCCS, no hidden state, no format change.  `make rev' stamps the set
# when the frame's $$R says $(REV)...
rev: ; for f in $(SHEETS); do cp $$f $$f.$(REV); done

# ...and `drift' asks -diff what has moved since it.  Its exit status is
# the change count, so print: and ps: above will not put an unreviewed
# sheet on paper.  `make review' hangs the answer on the wall instead:
# one markup drawing per sheet, printable by every backend.
drift: ; for f in $(SHEETS); do $(V) -diff $$f.$(REV) $$f; done

review: ; for f in $(SHEETS); do $(V) -diff -mark $$f.$(REV) $$f > $$f.mark; done

# THE SET AS A DOCUMENT (sec. 59): -book emits an ordinary drawing --
# a contents page, one row per sheet, each sheet's title taken from its
# frame stamp.  The sheets stay FILES; a book is a contents drawing plus
# a glob in the right order, not a container format.
book: ; $(V) -book $(SHEETS) > contents.d

doc: check book ; $(V) -ps contents.d $(SHEETS) | lpr

# A SHEET BIGGER THAN THE PAPER (sec. 58): -tile runs the backend once
# per page with the drawing origin stepped, one grid unit of overlap on
# each seam, crop marks and a "2/6 row B col 2" label -- the pages tape
# together.  -tile -n says how many first.  It does NOT compose with
# -fit, which means "make this ONE page".
pages: ; $(V) -tile -n -ps $(SHEETS)

big: check ; $(V) -tile -ps $(SHEETS) | lpr

# The STENCILS a shop draws with are drawings too, and their library is
# a build product with a gate on it: see /usr/vellum/etc/library.mk,
# whose worked example (/usr/vellum/eg/sk) rebuilds the stock pid
# library byte for byte from nine sketches.
#	vellum -mksym PUMP -pfx P pump.d >> pid.sym
#	vellum -symcheck pid.sym

# canonical designators, sheet by sheet (mind -base across the set):
#	vellum -renum -base 1 amp1.d > amp1.new && mv amp1.new amp1.d

# the library reference cards the shop binder wants:
#	vellum -symsheet /usr/vellum/sym/discrete.sym | vellum -ps -fit -

# a troff figure from a sheet; a sheet from the office CAD's DXF
.SUFFIXES: .d .pic .dxf
.d.pic: ; $(V) -pic $< > $@
.dxf.d: ; /usr/vellum/bin/veldxf $< > $@
