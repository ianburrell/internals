CC = m68k-palmos-gcc
CFLAGS = -g -O -Wall
LDFLAGS = -g 

name = "Internals"
crid = "Inrl"

all : Internals.prc

Internals.prc : internals internals.ro
	build-prc -o $@ -n $(name) -c $(crid) $^

internals : internals.o utils.o

internals.o : internals.c internals.h utils.h

utils.o: utils.c utils.h

internals.ro : internals.rcp internals.bmp 

%.ro : %.rcp
	pilrc $(PILRCFLAGS) -ro -o $@ $<

clean:
	-rm -f *.o internals *.bin *.prc *.ro *.zip


internals.zip: Internals.prc README.txt LICENSE.txt
	zip $@ $^

internals-src.zip: Internals.prc README.txt LICENSE.txt *.c *.h *.bmp *.rcp
	zip $@ $^

