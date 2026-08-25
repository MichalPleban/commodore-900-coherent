# project.mk - the vellum sheet-set Makefile skeleton (VELLUM.md sec. 40).
# A new design starts with a copy, the way a new title block already does:
#	cp /usr/vellum/etc/project.mk Makefile
# then set SHEETS.  check: and drift: gate print: and ps: the way cc gates
# on -Werror -- no paper moves until the set proves out and the changes
# since the approved revision have been looked at.
#
# Every tool named below is HEADLESS: the editor is a hi-res graphics
# client, but nothing here is, so a set builds on a machine with no
# bitmap card, over a serial line, from cron.  That is why the drawing
# tool and the drawing TOOLS are different programs.

V=/usr/vellum/bin
SHEETS=amp1.d amp2.d
REV=A
SPICE=deck.cir

check: ; $(V)/velcheck $(SHEETS)

print: check drift ; $(V)/velplot $(SHEETS) | lpr

ps: check drift ; $(V)/velplot -T ps -fit $(SHEETS) | lpr

plot: check ; $(V)/velplot -T hpgl $(SHEETS)

bom: check ; $(V)/velnet -bom $(SHEETS)

net: check ; $(V)/velnet $(SHEETS)

# the takeoff: run lengths per net and per layer, cut-out areas, a total
len: ; $(V)/velinfo -len $(SHEETS)

# the deck the department simulator reads; wrap it in a file that
# .INCLUDEs this one beside your .MODEL lines
spice: check ; $(V)/velnet -spice $(SHEETS) > $(SPICE)

# the drawing set's own -Werror for prose: fails while a sheet still
# carries a TODO note
todo: ; $(V)/velinfo -where TODO $(SHEETS)

# REVISIONS are files and the record is the directory (sec. 50): no
# SCCS, no hidden state, no format change.  `make rev' stamps the set
# when the frame's $$R says $(REV)...
rev: ; for f in $(SHEETS); do cp $$f $$f.$(REV); done

# ...and `drift' asks veldiff what has moved since it.  Its exit status
# is the change count, so print: and ps: above will not put an unreviewed
# sheet on paper.  `make review' hangs the answer on the wall instead:
# one markup drawing per sheet, printable by every backend.
drift: ; for f in $(SHEETS); do $(V)/veldiff $$f.$(REV) $$f; done

review: ; for f in $(SHEETS); do $(V)/veldiff -mark $$f.$(REV) $$f > $$f.mark; done

# THE SET AS A DOCUMENT (sec. 59): -book emits an ordinary drawing --
# a contents page, one row per sheet, each sheet's title taken from its
# frame stamp.  The sheets stay FILES; a book is a contents drawing plus
# a glob in the right order, not a container format.
book: ; $(V)/velinfo -book $(SHEETS) > contents.d

doc: check book ; $(V)/velplot -T ps contents.d $(SHEETS) | lpr

# A SHEET BIGGER THAN THE PAPER (sec. 58): -tile runs the backend once
# per page with the drawing origin stepped, one grid unit of overlap on
# each seam, crop marks and a "2/6 row B col 2" label -- the pages tape
# together.  -tile -n says how many first.  It does NOT compose with
# -fit, which means "make this ONE page".
pages: ; $(V)/velplot -tile -n -T ps $(SHEETS)

big: check ; $(V)/velplot -tile -T ps $(SHEETS) | lpr

# The STENCILS a shop draws with are drawings too, and their library is
# a build product with a gate on it: see /usr/vellum/etc/library.mk,
# whose worked example (/usr/vellum/eg/sk) rebuilds the stock pid
# library byte for byte from nine sketches.
#	velsym -pfx P PUMP pump.d >> pid.sym
#	velcheck -sym pid.sym

# canonical designators, sheet by sheet (mind -base across the set):
#	velnet -renum -base 1 amp1.d > amp1.new && mv amp1.new amp1.d

# the library reference cards the shop binder wants:
#	velinfo -symsheet /usr/vellum/sym/discrete.sym | velplot -T ps -fit -

# a troff figure from a sheet; a sheet from the office CAD's DXF
.SUFFIXES: .d .pic .dxf
.d.pic: ; $(V)/velpic $< > $@
.dxf.d: ; $(V)/veldxf $< > $@
