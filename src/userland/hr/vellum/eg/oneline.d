# Unit one-line -- the electrical stencils: generator, transformer,
# breakers, bus taps, disconnect, motor and the ground grid.
vellum1
T 30 6 s2 Unit one-line
Y GEN 40 12 0 0 G1 10MW
W 40 14 40 18
Y XFMR 40 21 0 0 T1 11kV
W 40 24 40 28
Y BRKR 40 30 0 0 Q1 1250A
W 40 32 40 37
Y BTAP 40 39 2 0 - -
W 37 39 32 39
W 32 39 32 42
Y GNDG 32 43 0 0 - -
W 43 39 53 39
Y BTAP 56 39 0 0 - -
W 59 39 68 39
Y BRKR 56 44 0 0 Q2 600A
W 56 41 56 42
W 56 46 56 48
Y MOT 56 50 0 0 M1 pump
W 68 39 68 42
Y DISC 68 44 0 0 S1 400A
W 68 46 68 48
Y MOT 68 50 0 0 M2 fan
