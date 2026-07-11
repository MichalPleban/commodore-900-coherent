CFLAGS= -f -O
OBJECTS=awk.o awk0.o awk1.o awk2.o awk3.o awk4.o awk5.o awk6.o

awk: $(OBJECTS)
	cc -fi -o awk $(OBJECTS) -lm

$(OBJECTS): awk.h

awk.o:	awk.y

awk1.o: y.tab.h
