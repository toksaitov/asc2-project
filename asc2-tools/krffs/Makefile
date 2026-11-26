CC=gcc
CFLAGS=-g -std=c11 -Wall -Wextra -Werror

FORMAT=../../asc2-rach2/sys/fs/krffs_format.h

.PHONY: all
all: asc_krffs

asc_krffs: asc_krffs.c $(FORMAT)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -rf asc_krffs asc_krffs.dSYM
