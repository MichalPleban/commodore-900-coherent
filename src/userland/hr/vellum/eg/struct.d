# Structure chart of Vellum itself -- the arch stencils; the dashed
# arrow is the spawned dialog helper.
vellum1
T 26 6 s2 Vellum, inside
Y MOD 40 14 0 0 M1 vellum
Y MOD 20 28 0 0 M2 velbase
Y MOD 40 28 0 0 M3 velfile
Y MOD 60 28 0 0 M4 velgfx
Y DMN 78 14 0 0 - veldlg
Y FILE 40 42 0 0 F1 draw.d
K arrow 40 15 20 27 @40,15 @20,27
K arrow 40 15 40 27 @40,15 @40,27
K arrow 40 15 60 27 @40,15 @60,27
K arrow 43 14 76 14 @43,14 @76,14 /1.0
K arrow 40 29 40 40 @40,29 @40,40
T 58 10 s0 /0.1 dialog helper
T 46 44 s0 /0.1 plain text file
