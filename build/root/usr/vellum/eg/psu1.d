# Supply, SHEET 1 of 2 -- a numbered sheet set: press > in the
# editor to walk to psu2.d (and < back).  The VBUS marker and the
# off-page stencil continue the rail on sheet 2:
#	vellum -net /usr/vellum/eg/psu1.d /usr/vellum/eg/psu2.d
# merges the named nets across the set and gives one netlist.
vellum1
T 26 12 s2 Supply, sheet 1
Y BAT 28 30 0 0 B1 9V
Y SW 34 30 0 0 S1 SPST
Y FUSE 42 30 0 0 F1 500mA
W 30 30 34 30
W 38 30 42 30
W 45 30 52 30
N 48 30 VBUS
Y OFFP 52 31 0 0 - >2
W 28 30 24 30
W 24 30 24 34
Y GND 24 34 0 0 - -
