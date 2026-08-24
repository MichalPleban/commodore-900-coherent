# general tolerance note -- the line every mechanical sheet carries.
# Two objects, not one: a T value is capped at 39 characters (the
# text pool's TVMAX), and a fragment that gets silently clipped is
# worse than one that needs two objects.  No leading blanks either:
# the parser's rest-of-line reader skips them, so an indent would
# survive inside a value and vanish at the start of one.
T 0 0 s1 /0.1 UNLESS NOTED:|LINEAR +/- 0.5
T 0 4 s1 /0.1 ANGULAR +/- 1 deg|BREAK EDGES
