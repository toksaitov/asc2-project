/*
    Functions for bytes and strings. `memset` and `memcpy` are part of the
    runtime library of the compiler, which calls them for its own needs too,
    so the kernel has no second copy of them. The others are in `kstring.c`.
*/

#ifndef _KSTRING_H_
#define _KSTRING_H_

#include <stddef.h>

// Sets `size` bytes at `dst` to the low byte of `value`. Returns `dst`.
void *memset(void *dst, int value, size_t size);

// Copies `size` bytes from `src` to `dst`. The two ranges must not overlap. Returns `dst`.
void *memcpy(void *dst, const void *src, size_t size);

// Compares `size` bytes. Returns 0 if they are equal.
int memcmp(const void *first, const void *second, size_t size);

// Returns the number of bytes of a string before its zero byte.
size_t strlen(const char *str);

// Compares two strings. Returns 0 if they are equal.
int strcmp(const char *first, const char *second);

#endif
