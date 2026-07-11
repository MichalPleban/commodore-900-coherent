

OBJECTS	= egrep.o nfa.o equiv.o dfa.o search.o bits.o

egrep: $(OBJECTS)
	cc -o egrep $(OBJECTS)

egrep.o:	egrep.h dragon.h
nfa.o:		egrep.h newt.h
equiv.o:	egrep.h newt.h eclass.h
dfa.o:		egrep.h newt.h dragon.h eclass.h
search.o:	egrep.h dragon.h
bits.o:		egrep.h
