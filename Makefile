CC=cc
CFLAGS=-g
LIBS=$(shell pkg-config --cflags --libs SoapySDR)

all: rx_fm rx_power rx_sdr

convenience.o: src/convenience/convenience.c src/convenience/convenience.h
	$(CC) $(CFLAGS) -c src/convenience/convenience.c -o convenience.o

rx_fm: convenience.o src/rtl_fm.c
	$(CC) $(CFLAGS) $(LIBS) src/rtl_fm.c convenience.o -o rx_fm

rx_power: convenience.o src/rtl_power.c
	$(CC) $(CFLAGS) $(LIBS) src/rtl_power.c convenience.o -o rx_power

rx_sdr: convenience.o src/rtl_sdr.c
	$(CC) $(CFLAGS) $(LIBS) src/rtl_sdr.c convenience.o -o rx_sdr

clean:
	rm -f *.o rx_fm rx_power rx_sdr

test:
	./rx_fm -M wbfm -f 107.3M | play -r 32k -t raw -e s -b 16 -c 1 -V1 -
