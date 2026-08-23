# project.mk - the vellum sheet-set Makefile skeleton (VELLUM.md sec. 40).
# A new design starts with a copy, the way a new title block already does:
#	cp /usr/vellum/etc/project.mk Makefile
# then set SHEETS.  check: gates print: and ps: the way cc gates on
# -Werror -- no paper moves until the set proves out.

SHEETS=amp1.d amp2.d

check: ; vellum -check $(SHEETS)

print: check ; vellum -print $(SHEETS) | lpr

ps: check ; vellum -ps -fit $(SHEETS) | lpr

plot: check ; vellum -hpgl $(SHEETS)

bom: check ; vellum -bom $(SHEETS)

net: check ; vellum -net $(SHEETS)

# canonical designators, sheet by sheet (mind -base across the set):
#	vellum -renum -base 1 amp1.d > amp1.new && mv amp1.new amp1.d

# a troff figure from a sheet; a sheet from the office CAD's DXF
.SUFFIXES: .d .pic .dxf
.d.pic: ; vellum -pic $< > $@
.dxf.d: ; veldxf $< > $@
