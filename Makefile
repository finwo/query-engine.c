SRC=$(wildcard src/*.c)
SRC+=test.c
BIN?=query-engine-test
CC?=gcc

TAIL=$(shell command -v gtail tail | head -1)
HEAD=$(shell command -v ghead head | head -1)

override CFLAGS?=-Wall -s -O2

include lib/.dep/config.mk

.PHONY: default
default: README.md ${BIN}

${BIN}: ${SRC} src/query-engine.h
	${CC} -Isrc ${INCLUDES} ${CFLAGS} -o $@ ${SRC}

.PHONY: check
check: ${BIN}
	./$<

.PHONY: clean
clean:
	rm -f ${BIN}

README.md: ${SRC} src/query-engine.h
	stddoc < src/query-engine.h > README.md
