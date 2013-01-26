CC=gcc
CFLAGS=-O2 -g -Wall -g
PREFIX=/usr/local
NAME=sbar

sbar: sbar.o
	gcc ${CFLAGS} external/parson.c sbar.o -lX11 -lasound -lsensors -lm -o sbar
sbar.o: sbar.c
	gcc ${CFLAGS} -c sbar.c

clean:
	rm -f sbar.o ${NAME} parson.o

install:
	@cp sbar ${PREFIX}/bin/sbar

uninstall:
	@rm ${PREFIX}/bin/sbar

