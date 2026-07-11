CFLAGS=-O
sh: main.o alloc.o eval.o exec1.o exec2.o exec3.o extern.o lex.o malloc.o tab.o trap.o var.o y.tab.o atoi.o glob.o
	cc -i -o sh main.o alloc.o eval.o exec1.o exec2.o exec3.o extern.o lex.o malloc.o tab.o trap.o var.o y.tab.o atoi.o glob.o

psh: main.o alloc.o eval.o exec1.o exec2.o exec3.o extern.o lex.o malloc.o tab.o trap.o var.o y.tab.o atoi.o glob.o
	cc -n -o psh -O -DPARANOID main.c alloc.o eval.o exec1.o exec2.o exec3.o extern.o lex.o malloc.o tab.o trap.o var.o y.tab.o atoi.o glob.o

any:
	@: dummy entry
alloc.o: sh.h alloc.c
eval.o: sh.h eval.c
exec1.o: sh.h exec1.c
exec2.o: sh.h exec2.c
exec3.o: sh.h exec3.c
extern.o: sh.h extern.c
lex.o: sh.h y.tab.h lex.c
main.o: sh.h main.c
malloc.o: malloc.c
tab.o: sh.h tab.c
trap.o: sh.h trap.c
var.o: sh.h var.c any
	cc -O -c -DVERSION='"VERSION='"`version`"'"' var.c
y.tab.o: sh.h y.tab.h
	cc -O -c y.tab.c
y.tab.h: sh.y
	yacc sh.y
atoi.o: atoi.c
glob.o: sh.h glob.c
