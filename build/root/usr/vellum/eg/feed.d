# Feed system P&ID -- the process stencils: tank, pump, check and
# gate valves, heat exchanger, vessel, control valve, and a dashed
# instrument signal.
vellum1
T 28 10 s2 Feed system
Y TANK 24 30 0 0 TK1 feed
W 26 30 34 30
Y PUMP 36 30 0 0 P1 -
Y INST 36 22 0 0 I1 PI
L 36 24 36 28 /1.1
W 38 30 44 30
Y VCHK 46 30 0 0 V1 -
W 48 30 55 30
Y HX 58 30 0 0 E1 cooler
W 61 30 64 30
Y VGATE 66 30 0 0 V2 -
W 68 30 72 30
W 72 30 72 33
Y VESS 72 36 0 0 VS1 -
W 72 39 72 44
W 72 44 74 44
Y VCTL 76 44 0 0 V3 -
W 78 44 84 44
K arrow 84 44 90 44 - -
T 85 41 s0 /0.1 to plant
