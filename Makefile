CC=gcc
CFLAGS=-O2 -g -Wall -g
PREFIX=/usr/local
NAME=sbar

SRC=external/parson.c sbar.c
OBJ=parson.o sbar.o

all: options sbar

options:
	@echo sbar build options:
	@echo "CFLAGS = ${CFLAGS}"
	@echo "CC	  = ${CC}"

.c.o:
	@echo ${CC} -c ${CFLAGS} ${SRC}
	@${CC} -c ${CFLAGS} ${SRC}

${OBJ}: config.h

sbar: ${OBJ}
	@echo ${CC} -o $@ ${OBJ} -lX11 -lasound -lsensors -lm
	@${CC} -o $@ ${OBJ} -lX11 -lasound -lsensors -lm

clean:
	@echo 
	@rm -f sbar.o ${NAME} parson.o

install:
	@cp sbar ${PREFIX}/bin/sbar

uninstall:
	@rm ${PREFIX}/bin/sbar

