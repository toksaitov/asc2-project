/*
    Functions for text: the few that the programs of the disk need.
*/

#include "user.h"

#define MAX_DIGITS 10 // decimal digits of the largest 32-bit number

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }

    return len;
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }

    return (unsigned char) *a - (unsigned char) *b;
}

void print(int fd, const char *str)
{
    write(fd, str, strlen(str));
}

void print_number(int fd, unsigned int number)
{
    char digits[MAX_DIGITS];

    // The last digit is found first, so the digits fill the array from its end
    int first = MAX_DIGITS;
    do {
        digits[--first] = (char) ('0' + number % 10);
        number /= 10;
    } while (number != 0);

    write(fd, &digits[first], MAX_DIGITS - first);
}
