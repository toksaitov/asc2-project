CC=gcc
CFLAGS=-g -std=c11 -I ../asc2-emulated

.PHONY: all
all: asa

asa: asa.c ../asc2-emulated/asc.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

.PHONY: clean
clean:
	rm -rf asa asa.dSYM
