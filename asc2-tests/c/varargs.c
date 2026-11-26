/*
    Functions with a variable number of arguments.

    The arguments of a call are on the stack one after another, each in a
    whole number of words, so a function can walk them with a pointer.
    `va_start` takes the address after the last named argument, and `va_arg`
    reads a value and moves the pointer by its size. A `char` or a `short`
    is passed as an `int`, and a `long long` takes two words.
*/

#include <stdarg.h>

#include "out.h"

static int sum(int count, ...)
{
    va_list args;
    int total = 0;

    va_start(args, count);
    for (int i = 0; i < count; ++i) {
        total += va_arg(args, int);
    }
    va_end(args);

    return total;
}

static long long sum_wide(int count, ...)
{
    va_list args;
    long long total = 0;

    va_start(args, count);
    for (int i = 0; i < count; ++i) {
        total += va_arg(args, long long);
    }
    va_end(args);

    return total;
}

/*
    Prints the text with these marks replaced by the next argument:
    %d a number with a sign, %u a number without one, %x a number in
    hexadecimal, %l a 64-bit number in hexadecimal, %c a character,
    %s a string, %% a percent sign.
*/
static void print_list(const char *format, va_list args)
{
    for (; *format != '\0'; ++format) {
        if (*format != '%') {
            put_char(*format);
            continue;
        }

        ++format;
        switch (*format) {
            case 'd':
                put_dec(va_arg(args, int));
                break;
            case 'u': {
                unsigned int value = va_arg(args, unsigned int);
                if (value >= 1000000000u) {
                    put_dec((int) (value / 1000000000u));
                    value %= 1000000000u;
                    for (unsigned int digit = 100000000u; digit != 0; digit /= 10) {
                        put_char((int) ('0' + value / digit % 10));
                    }
                } else {
                    put_dec((int) value);
                }
                break;
            }
            case 'x':
                put_hex(va_arg(args, unsigned int));
                break;
            case 'l':
                put_hex64(va_arg(args, unsigned long long));
                break;
            case 'c':
                put_char(va_arg(args, int));
                break;
            case 's':
                put_str(va_arg(args, const char *));
                break;
            default:
                put_char(*format);
                break;
        }
    }
}

static void print(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    print_list(format, args);
    va_end(args);
}

/* Passes its own list on, with one more argument taken first. */
static void print_numbered(int number, const char *format, ...)
{
    va_list args;

    put_dec(number);
    put_str(": ");
    va_start(args, format);
    print_list(format, args);
    va_end(args);
}

int main(void)
{
    signed char byte = -7;
    short half = -300;
    long long wide = 0x0123456789abcdefLL;

    show("sum of none", sum(0));
    show("sum of one", sum(1, 42));
    show("sum of five", sum(5, 1, -2, 3, -4, 50000));
    show("sum of bytes and shorts", sum(3, byte, half, 'A'));
    show64("sum_wide", sum_wide(3, wide, 1LL, -2LL));
    show64("sum_wide carry", sum_wide(2, 0xffffffffLL, 1LL));

    print("plain text\n");
    print("%d %d %d\n", 0, -1, 2147483647);
    print("%d\n", (int) 0x80000000);
    print("%u %u\n", 4294967295u, 3000000000u);
    print("%x %x\n", 0xdeadbeef, 255);
    print("%l\n", wide);
    print("%c%c%c %s and %s\n", 'a', 'b', 'c', "first", "second");
    print("100%% of %s\n", "it");
    print("%d %l %d %s %l %c\n", 1, wide, 2, "between", -1LL, '!');
    print_numbered(7, "%s = %d\n", "value", byte);
    print_numbered(8, "%x\n", half);

    return 0;
}
