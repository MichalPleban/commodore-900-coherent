# Common-emitter amplifier -- the schematic showcase: supply rails,
# junction dots, net names, designators and values.  Try
#	vellum -net /usr/vellum/eg/amp.d
# and watch VCC/GND/IN/OUT come out as named nets.
vellum1
T 32 8 s2 Common emitter
Y VCC 44 14 0 0 - -
Y VCC 52 14 0 0 - -
Y R 44 18 1 0 R1 47k
Y R 52 18 1 0 R2 4k7
Y NPN 50 30 0 0 Q1 BC237
Y R 44 34 1 0 R3 10k
Y R 52 34 1 0 R4 1k
Y GND 44 40 0 0 - -
Y GND 52 40 0 0 - -
Y C 36 30 0 0 C1 100n
Y C 56 26 0 0 C2 100n
Y J 28 30 0 0 J1 in
Y J 64 26 2 0 J2 out
W 44 14 44 18
W 52 14 52 18
W 44 22 44 30
W 44 30 44 34
W 44 30 50 30
W 52 22 52 28
W 52 26 56 26
W 52 32 52 34
W 44 38 44 40
W 52 38 52 40
W 30 30 36 30
W 38 30 44 30
W 58 26 62 26
N 32 30 IN
N 60 26 OUT
