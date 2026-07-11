OBJ=ansi.o basic.o buffer.o display.o file.o fileio.o line.o main.o \
    random.o region.o search.o spawn.o termio.o vt52.o window.o word.o

emacs:	$(OBJ)
	cc -n -o emacs $(OBJ)

$(OBJ):	ed.h
