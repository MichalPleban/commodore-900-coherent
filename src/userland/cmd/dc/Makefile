bc=/usr/src/cmd/bc
CFLAGS=-OI$(bc)
bco=bcmch.o bcmutil.o getnum.o globals.o output.o putnum.o
dc:	dc.o dcsub.o $(bco) undefined.o
	$(CC) $(CFLAGS) *.o -lmp -o dc
bcmch.o: $(bc)/bcmch.c
	$(CC) $(CFLAGS) -c $?
bcmutil.o: $(bc)/bcmutil.c
	$(CC) $(CFLAGS) -c $?
getnum.o: $(bc)/getnum.c
	$(CC) $(CFLAGS) -c $?
globals.o: $(bc)/globals.c
	$(CC) $(CFLAGS) -c $?
output.o: $(bc)/output.c
	$(CC) $(CFLAGS) -c $?
putnum.o: $(bc)/putnum.c
	$(CC) $(CFLAGS) -c $?
