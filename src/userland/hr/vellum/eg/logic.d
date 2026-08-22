# Half adder -- the logic library: gates, crossing wires (no dot,
# no connection), T junctions (dot).  `vellum -net' names the nets
# after the A/B/SUM/CY markers.
vellum1
T 24 12 s2 Half adder
Y XOR 30 20 0 0 U1 -
Y AND 30 28 0 0 U2 -
W 20 20 30 20
W 24 20 24 28
W 24 28 30 28
W 20 22 30 22
W 26 22 26 30
W 26 30 30 30
N 20 20 A
N 20 22 B
W 35 21 40 21
W 35 29 40 29
N 38 21 SUM
N 39 29 CY
Y J 42 21 2 0 J1 -
Y J 42 29 2 0 J2 -
