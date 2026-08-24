# library.mk - a stencil LIBRARY as a build product (VELLUM.md sec. 57).
# The companion of project.mk: that one makes a drawing SET prove out
# before paper moves, this one makes the stencils a shop draws WITH a
# thing that is built, checked and revised like everything else.
#
#	cp /usr/vellum/etc/library.mk Makefile
#
# then set LIB and the stencil list.  Nothing here is a new mechanism:
# a stencil SKETCH is an ordinary drawing, so it opens in the editor,
# prints, -pic's into the manual, and takes v5's whole revision workflow
# (rev:/drift:/review: in project.mk) unchanged.  The shop's stencils get
# revision control because they are drawings.
#
# The worked example is the one that ships: /usr/vellum/eg/sk holds the
# nine sketches that BUILD /usr/vellum/sym/pid.sym, and `make pid.sym'
# below reproduces that library byte for byte -- which is also the
# converter's fix point (sec. 62).

# V names the EXPORTER, not the editor.  `vellum -mode' hands its
# arguments straight to this program anyway (main() execs it for any
# lowercase -mode), and the editor is a hi-res GRAPHICS client: on a
# machine with no bitmap card it cannot even exec, because the shared
# graphics library it links is not loaded there.  The exporter is the
# headless half and runs anywhere, which is what a Makefile wants.
# Say V=/usr/vellum/bin/vellum if you would rather type the front door
# and you know the machine has a screen.
V=/usr/vellum/lib/velxport
SK=/usr/vellum/eg/sk
LIB=pid.sym

# The library's own header comment is a FILE ($(SK)/HEADER), not an
# echoed line: make(1) eats a # to end of line even inside a recipe,
# so a comment a Makefile wants to WRITE has to arrive from a file.

# The sketches are drawn at -scale 4 (one drawing unit IS one quarter
# unit) so a hand-cut stencil's detail is available, and -org names the
# grid point that becomes the symbol origin -- the point it snaps by.
# A sketch whose FIRST N marker is the origin needs no -org at all.
ORG=-org 40,30 -scale 4

$(LIB): $(SK)/pump.d $(SK)/vgate.d $(SK)/vchk.d $(SK)/vctl.d \
		$(SK)/tank.d $(SK)/vess.d $(SK)/hx.d $(SK)/comp.d \
		$(SK)/inst.d
	rm -f $@
	cat $(SK)/HEADER > $@
	$(V) -mksym PUMP  -pfx P  $(ORG) $(SK)/pump.d  >> $@
	$(V) -mksym VGATE -pfx V  $(ORG) $(SK)/vgate.d >> $@
	$(V) -mksym VCHK  -pfx V  $(ORG) $(SK)/vchk.d  >> $@
	$(V) -mksym VCTL  -pfx V  $(ORG) $(SK)/vctl.d  >> $@
	$(V) -mksym TANK  -pfx TK $(ORG) $(SK)/tank.d  >> $@
	$(V) -mksym VESS  -pfx VS $(ORG) $(SK)/vess.d  >> $@
	$(V) -mksym HX    -pfx E  $(ORG) $(SK)/hx.d    >> $@
	$(V) -mksym COMP  -pfx K  $(ORG) $(SK)/comp.d  >> $@
	$(V) -mksym INST  -pfx I  $(ORG) $(SK)/inst.d  >> $@
	$(V) -symcheck $@

# -symcheck is the library's -check: one finding per line, the exit
# status the finding count, so make(1) gates on it the same way and a
# wrong stencil never reaches a drawing.  A wrong stencil is worse than
# a wrong drawing, because it is wrong in EVERY drawing.
check: ; $(V) -symcheck $(LIB)

# every library the machine loads, judged together -- the cross-library
# duplicate ("code R defined in both discrete.sym and power.sym") is the
# finding no single file can produce
checkall: ; $(V) -symcheck /usr/vellum/sym/*.sym

# the reference card the shop binder wants (v5.3), on the laser
card: $(LIB) ; $(V) -symsheet $(LIB) | $(V) -ps -fit -

# a stencil sketch is a DRAWING: it prints like one...
sketches: ; $(V) -ps -fit $(SK)/*.d

# ...and figures for the manual fall out of it
$(SK)/pump.pic: $(SK)/pump.d ; $(V) -pic $(SK)/pump.d > $@

# Installing is a copy: adding a stencil is adding a line above, and
# deleting one is deleting a file.  There is no library format to learn,
# no tool to run interactively, and no state anywhere but files.
install: $(LIB) ; cp $(LIB) /usr/vellum/sym/$(LIB)
