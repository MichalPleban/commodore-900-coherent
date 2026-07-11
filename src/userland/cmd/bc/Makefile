YFLAGS	= -hdr yy.h -st -terms 55 -nterms 34 -prods 109 -states 203
OFILES	= bcmch.o  bcmutil.o getnum.o globals.o gram.o grmact.o\
		interp.o lex.o main.o output.o putnum.o
HFILES	= bc.h bcmch.h bcsymtbl.h
SFILES	= globals.c main.c gram.y grmact.c interp.c bcmch.c bcmutil.c\
		lex.c getnum.c putnum.c output.c
MFILES	= readme makefile

l.out:		$(OFILES)
	@echo old version size:; size /bin/bc
	$(CC) $(CFLAGS) -o bc $(OFILES) -lmp
	@echo new version size:; size bc

bcprof:		$(OFILES)
	$(CC) $(CFLAGS) -o bcprof -p $(OFILES) -lmp

print:
	pr $(MFILES) $(HFILES) $(SFILES)

ofiles:		$(OFILES)

clean:
	-rm $(OFILES) yy.h

$(OFILES):	$(HFILES)

lex.o:		gram.o
