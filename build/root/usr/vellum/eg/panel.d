# Sheet-metal front panel -- the TAKEOFF demo (VELLUM.md sec. 46).
# A closed FILLED polyline is an AREA, so
#	velinfo -len /usr/vellum/eg/panel.d
# orders the plate and the cut-outs without anyone retyping a number:
# the plate is 24000 mm2, the window 1200, the vent 1237.5 (the odd
# half a shoelace leaves, shown as ".5" -- fixed point, no floats).
vellum1
U 5 mm
T 10 4 s2 Front panel
P 4 10 10 50 10 50 34 10 34 /4.0
P 4 18 16 26 16 26 22 18 22 /12.0
P 3 32 16 43 16 42 25 /8.0
T 18 23 s0 /0.1 display window
T 32 26 s0 /0.1 vent
D 10 36 50 36
D 8 10 8 34
