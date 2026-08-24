# Office LAN map -- the network stencils on an ethernet bus, with
# an arrow connector off to the WAN.
vellum1
T 26 8 s2 Office network
K hv 20 24 76 24 - - /16.0
Y TAP 28 24 0 0 - -
Y TAP 44 24 0 0 - -
Y TAP 60 24 0 0 - -
K hv 28 26 28 32 - -
K hv 44 26 44 32 - -
K hv 60 26 60 32 - -
Y HOST 28 34 0 0 H1 server
T 20 37 s0 /0.1 files
Y VDU 44 34 0 0 T1 desk
Y PRT 60 34 0 0 P1 laser
K hv 28 36 28 40 - -
Y DSK 28 43 0 0 D1 disks
Y GW 78 24 0 0 G1 router
K hv 80 24 82 24 - -
Y MDM 84 24 0 0 M1 9600bps
K arrow 86 24 92 26 - -
T 88 21 s0 WAN
