SRC=$(wildcard src/*.c)
CC?=gcc

BIN=\
	benchmark \
	test

TAIL=$(shell command -v gtail tail | head -1)
HEAD=$(shell command -v ghead head | head -1)

override CFLAGS?=-Wall -O2
override LDFLAGS?=-s

include lib/.dep/config.mk

override CFLAGS+=$(INCLUDES)
override CFLAGS+=-Isrc

OBJ=$(SRC:.c=.o)

BINO=$(BIN:=.o)

.PHONY: default
default: README.md ${BIN}

.c.o:
	$(CC) $< $(CFLAGS) -c -o $@

${BIN}: ${OBJ} ${BINO} src/query-engine.h
	${CC} -Isrc ${INCLUDES} ${CFLAGS} -o $@ ${@}.o ${OBJ}

.PHONY: check
check: ${BIN}
	./$<

.PHONY: clean
clean:
	rm -f ${BIN}
	rm -f ${OBJ}

README.md: ${SRC} src/query-engine.h
	stddoc < src/query-engine.h > README.md
