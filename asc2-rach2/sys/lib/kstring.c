/*
    Functions for bytes and strings.
*/

#include "sys/lib/kstring.h"

int memcmp(const void *first, const void *second, size_t size)
{
    const unsigned char *first_bytes = first;
    const unsigned char *second_bytes = second;

    for (size_t i = 0; i < size; ++i) {
        if (first_bytes[i] != second_bytes[i]) {
            return first_bytes[i] < second_bytes[i] ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }

    return len;
}

int strcmp(const char *first, const char *second)
{
    while (*first != '\0' && *first == *second) {
        ++first;
        ++second;
    }

    return (unsigned char) *first - (unsigned char) *second;
}
