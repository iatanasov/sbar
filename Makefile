CC=gcc
CFLAGS=-Wall

sbar: sbar.o
	gcc -g sbar.o -lX11 -lasound -lsensors -lm -o sbar
sbar.o: sbar.c
	gcc -g -c sbar.c

clean:
	rm -f sbar.o sbar

install:
	cp sbar ~/bin/sbar
