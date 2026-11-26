CC=gcc
CFLAGS=-g -std=c11

.PHONY: all
all: asc_elf2fw

asc_elf2fw: asc_elf2fw.c ../../asc2-emulated/asc.h
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -rf asc_elf2fw asc_elf2fw.dSYM
