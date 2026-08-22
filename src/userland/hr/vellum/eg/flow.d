# Rebuild-loop flowchart -- stretchable shapes with centred labels
# and ATTACHED arrow connectors: move any shape in the editor and
# the arrows re-route with it.
vellum1
T 28 2 s2 Rebuild loop
S oval 32 6 48 12 Start
K harrow 40 12 40 16 @40,12 @40,16
S box 28 16 52 24 Read rules
K harrow 40 24 40 28 @40,24 @40,28
S diamond 24 28 56 40 Stale?
K harrow 56 34 75 24 @56,34 @75,24
T 57 31 s0 yes
S box 64 16 86 24 Rebuild it
K harrow 64 20 52 20 @64,20 @52,20
K harrow 40 40 40 44 @40,40 @40,44
T 41 41 s0 no
S oval 32 44 48 50 Done
