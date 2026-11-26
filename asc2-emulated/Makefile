CC=gcc
CFLAGS=-g -std=c11
LDLIBS=-lncurses

.PHONY: all
all: asc

asc: asc.c asc.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

.PHONY: clean
clean:
	rm -rf asc asc.dSYM
