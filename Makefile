CC=cc
CFLAGS=
LIBS=$(shell pkg-config --cflags --libs librtlsdr)
LIBS+=$(shell pkg-config --cflags --libs SoapySDR)

all: rtl_fm rtl_power rtl_sdr

convenience.o: src/convenience/convenience.c src/convenience/convenience.h
	$(CC) $(CFLAGS) -c src/convenience/convenience.c -o convenience.o

convenience-rtl.o: src/convenience/convenience-rtl.c src/convenience/convenience-rtl.h
	$(CC) $(CFLAGS) -c src/convenience/convenience-rtl.c -o convenience-rtl.o

rtl_fm: convenience.o src/rtl_fm.c
	$(CC) $(CFLAGS) $(LIBS) src/rtl_fm.c convenience.o -o rtl_fm

rtl_power: convenience-rtl.o src/rtl_power.c
	$(CC) $(CFLAGS) $(LIBS) src/rtl_power.c convenience-rtl.o -o rtl_power

rtl_sdr: convenience.o src/rtl_sdr.c
	$(CC) $(CFLAGS) $(LIBS) src/rtl_sdr.c convenience-rtl.o -o rtl_sdr

clean:
	rm -f *.o rtl_fm rtl_power rtl_sdr

test:
	./rtl_fm -M wbfm -f 107.1M | play -r 32k -t raw -e s -b 16 -c 1 -V1 -
