CC=cc
CFLAGS=
LIBS=$(shell pkg-config --cflags --libs librtlsdr)

all: rtl_fm rtl_power rtl_sdr

convenience.o:
	$(CC) $(CFLAGS) -c src/convenience/convenience.c -o convenience.o

rtl_fm: convenience.o
	$(CC) $(CFLAGS) $(LIBS) src/rtl_fm.c convenience.o -o rtl_fm

rtl_power: convenience.o
	$(CC) $(CFLAGS) $(LIBS) src/rtl_power.c convenience.o -o rtl_power

rtl_sdr: convenience.o
	$(CC) $(CFLAGS) $(LIBS) src/rtl_sdr.c convenience.o -o rtl_sdr

clean:
	rm -f *.o rtl_fm rtl_power rtl_sdr

