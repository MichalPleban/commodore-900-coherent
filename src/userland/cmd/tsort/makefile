OFILES	= alc.o hash.o input.o logic.o main.o util.o
CFLAGS	= -O

tsort:	$(OFILES)
	$(CC) $(CFLAGS) -o tsort $(OFILES)

clean:
	-rm $(OFILES)

$(OFILES):	tsort.h
