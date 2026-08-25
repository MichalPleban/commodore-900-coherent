# library.mk - a stencil LIBRARY as a build product (VELLUM.md sec. 57).
# The companion of project.mk: that one makes a drawing SET prove out
# before paper moves, this one makes the stencils a shop draws WITH a
# thing that is built, checked and revised like everything else.
#
#	cp /usr/vellum/etc/library.mk Makefile
#
# then set LIB and the stencil list.  Nothing here is a new mechanism:
# a stencil SKETCH is an ordinary drawing, so it opens in the editor,
# prints, velpic's into the manual, and takes the whole revision
# workflow (rev:/drift:/review: in project.mk) unchanged.  The shop's
# stencils get revision control because they are drawings.
#
# The worked example is the one that ships: /usr/vellum/eg/sk holds the
# nine sketches that BUILD /usr/vellum/sym/pid.sym, and `make pid.sym'
# below reproduces that library byte for byte -- which is also the
# converter's fix point (sec. 62).

V=/usr/vellum/bin
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
	$(V)/velsym -pfx P  $(ORG) PUMP  $(SK)/pump.d  >> $@
	$(V)/velsym -pfx V  $(ORG) VGATE $(SK)/vgate.d >> $@
	$(V)/velsym -pfx V  $(ORG) VCHK  $(SK)/vchk.d  >> $@
	$(V)/velsym -pfx V  $(ORG) VCTL  $(SK)/vctl.d  >> $@
	$(V)/velsym -pfx TK $(ORG) TANK  $(SK)/tank.d  >> $@
	$(V)/velsym -pfx VS $(ORG) VESS  $(SK)/vess.d  >> $@
	$(V)/velsym -pfx E  $(ORG) HX    $(SK)/hx.d    >> $@
	$(V)/velsym -pfx K  $(ORG) COMP  $(SK)/comp.d  >> $@
	$(V)/velsym -pfx I  $(ORG) INST  $(SK)/inst.d  >> $@
	$(V)/velcheck -sym $@

# `velcheck -sym' is the library's velcheck: one finding per line, the
# exit status the finding count, so make(1) gates on it the same way and
# a wrong stencil never reaches a drawing.  A wrong stencil is worse
# than a wrong drawing, because it is wrong in EVERY drawing.
check: ; $(V)/velcheck -sym $(LIB)

# every library the machine loads, judged together -- the cross-library
# duplicate ("code R defined in both discrete.sym and power.sym") is the
# finding no single file can produce
checkall: ; $(V)/velcheck -sym /usr/vellum/sym/*.sym

# the reference card the shop binder wants (v5.3), on the laser
card: $(LIB) ; $(V)/velinfo -symsheet $(LIB) | $(V)/velplot -T ps -fit -

# a stencil sketch is a DRAWING: it prints like one...
sketches: ; $(V)/velplot -T ps -fit $(SK)/*.d

# ...and figures for the manual fall out of it
$(SK)/pump.pic: $(SK)/pump.d ; $(V)/velpic $(SK)/pump.d > $@

# Installing is a copy: adding a stencil is adding a line above, and
# deleting one is deleting a file.  There is no library format to learn,
# no tool to run interactively, and no state anywhere but files.
install: $(LIB) ; cp $(LIB) /usr/vellum/sym/$(LIB)
