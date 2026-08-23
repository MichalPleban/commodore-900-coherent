# Vellum frame template (/usr/vellum/etc/frame.d): the Frame button feeds
# these ordinary .d lines through the drawing parser onto the FRAME layer,
# substituting in text values:
#   $F  file name        $D  date ("Aug 23 2026")
#   $S  "Sheet n/m" (empty when the file name carries no sheet number)
#   $R  revision
# Edit this file to make it your own title block -- coordinates are grid
# units on the default 160x120 sheet; keep an A4 variant beside it and
# copy it over when your shop standardizes on that sheet.
B 1 1 159 119 /16.2
B 115 113 159 119 /0.2
L 115 116 159 116 /0.2
T 116 113 s2 /0.2 $F
T 116 116 s2 /0.2 $D
T 140 113 s2 /0.2 $S
T 151 116 s2 /0.2 $R
