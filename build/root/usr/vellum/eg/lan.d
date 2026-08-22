# Office LAN map -- the network stencils on an ethernet bus, with
# an arrow connector off to the WAN.
vellum1
T 26 8 s2 Office network
W 20 24 76 24
Y TAP 28 24 0 0 - -
Y TAP 44 24 0 0 - -
Y TAP 60 24 0 0 - -
W 28 26 28 32
W 44 26 44 32
W 60 26 60 32
Y HOST 28 34 0 0 H1 -
T 20 37 s0 /0.1 files
Y TERM 44 34 0 0 T1 desk
Y PRT 60 34 0 0 P1 laser
W 28 36 28 40
Y DSK 28 43 0 0 D1 -
Y GW 78 24 0 0 G1 router
W 80 24 82 24
Y MDM 84 24 0 0 M1 -
K arrow 86 24 92 26 - -
T 88 21 s0 WAN
