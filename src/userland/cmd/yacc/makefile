CFLAGS=-O

yacc: y0.o y1.o y2.o y3.o y4.o y5.o y6.o 
	$(CC) $(CFLAGS) -i -o yacc y?.o 
	rm y?.o

y0.o y1.o y4.o: yacc.h
y3.o: yacc.h assert.h
y2.o y6.o: yacc.h action.h
y5.o: yacc.h assert.h action.h
