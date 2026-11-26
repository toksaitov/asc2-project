#include "out.h"

void put_str(const char *str)
{
    while (*str != '\0') {
        put_char(*str);
        ++str;
    }
}

void put_hex(unsigned int value)
{
    for (int shift = 28; shift >= 0; shift -= 4) {
        put_char("0123456789abcdef"[(value >> shift) & 0xf]);
    }
}

void put_hex64(unsigned long long value)
{
    put_hex((unsigned int) (value >> 32));
    put_hex((unsigned int) value);
}

void put_dec(int value)
{
    char digits[10];
    int count = 0;

    /* The magnitude is kept as an unsigned number, because the lowest
       negative number has no positive twin. */
    unsigned int rest = value < 0 ? 0u - (unsigned int) value : (unsigned int) value;

    do {
        digits[count++] = (char) ('0' + rest % 10);
        rest /= 10;
    } while (rest != 0);

    if (value < 0) {
        put_char('-');
    }
    while (count > 0) {
        put_char(digits[--count]);
    }
}

void show(const char *name, unsigned int value)
{
    put_str(name);
    put_str(" = 0x");
    put_hex(value);
    put_char('\n');
}

void show64(const char *name, unsigned long long value)
{
    put_str(name);
    put_str(" = 0x");
    put_hex64(value);
    put_char('\n');
}
