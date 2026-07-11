CC = cc
CFLAGS = -O

HFILES = knapsack.h /usr/include/mprec.h
ENROBJ = gpph.o knapsack.o public.o pkio.o enroll.o
XENOBJ = gpph.o knapsack.o public.o pkio.o xencode.o
XDEOBJ = gpph.o knapsack.o xdecode.o
ALLOBJ = $(ENROBJ) xencode.o xdecode.o

SU = su root
PUBKEY = /usr/spool/pubkey

all: enroll xdecode xencode
	chmod u+s enroll xdecode xencode
	$(SU) mv enroll xdecode xencode /bin
	$(SU) chown xmail /bin/enroll /bin/xdecode /bin/xencode
	rm *.o
	@: Now check the public key file.
	@if test -f $(PUBKEY); \
	then	echo $(PUBKEY) is not a directory.; \
		exit 1; \
	elif test ! -d $(PUBKEY); \
	then	$(SU) mkdir $(PUBKEY); \
		$(SU) chmod 0700 $(PUBKEY); \
		$(SU) chown xmail $(PUBKEY); \
	fi

enroll: $(ENROBJ) /usr/lib/libmp.a
	cc -O -o enroll $(ENROBJ) -lmp

xencode: $(XENOBJ) /usr/lib/libmp.a
	cc -O -o xencode $(XENOBJ) -lmp

xdecode: $(XDEOBJ) /usr/lib/libmp.a
	cc -O -o xdecode $(XDEOBJ) -lmp

$(ALLOBJ): $(HFILES)
