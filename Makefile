CC=gcc
CFLAGS=-Wall
PREFIX=/usr/local

sbar: sbar.o
	gcc -g sbar.o -lX11 -lasound -lsensors -lm -o sbar -ljansson
sbar.o: sbar.c
	gcc -g -c sbar.c

clean:
	rm -f sbar.o sbar

install:
	@cp sbar ${PREFIX}/bin/sbar

uninstall:
	@rm ${PREFIX}/bin/sbar
