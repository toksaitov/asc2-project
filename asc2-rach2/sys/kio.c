/*
    Messages of the kernel on the console.
*/

#include "sys/kio.h"

#include <stdarg.h>
#include <stdbool.h>

#include "sys/console.h"
#include "sys/lib/kint.h"

#define MAX_DIGITS 10 // digits of the largest 32-bit number in decimal

static const char HEX_DIGITS[] = "0123456789abcdef";

void k_putc(char ch)
{
    console_putc(ch);
}

void k_puts(const char *str)
{
    while (*str != '\0') {
        k_putc(*str++);
    }
}

static void print_unsigned(uint32_t value)
{
    char digits[MAX_DIGITS];
    int count = 0;

    // The division gives the last digit first
    do {
        digits[count++] = (char) ('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    while (count > 0) {
        k_putc(digits[--count]);
    }
}

static void print_hex(uint32_t value, bool all_digits)
{
    bool is_started = all_digits;

    for (int shift = 28; shift >= 0; shift -= 4) {
        uint32_t digit = (value >> shift) & 0xf;
        if (digit != 0 || shift == 0) {
            is_started = true;
        }
        if (is_started) {
            k_putc(HEX_DIGITS[digit]);
        }
    }
}

void k_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    while (*format != '\0') {
        char ch = *format++;
        if (ch != '%') {
            k_putc(ch);
            continue;
        }

        ch = *format++;
        if (ch == 'c') {
            k_putc((char) va_arg(args, int));
        } else if (ch == 's') {
            k_puts(va_arg(args, const char *));
        } else if (ch == 'd') {
            int32_t value = va_arg(args, int32_t);
            if (value < 0) {
                k_putc('-');
                print_unsigned(0 - (uint32_t) value);
            } else {
                print_unsigned((uint32_t) value);
            }
        } else if (ch == 'u') {
            print_unsigned(va_arg(args, uint32_t));
        } else if (ch == 'x') {
            print_hex(va_arg(args, uint32_t), false);
        } else if (ch == 'p') {
            k_puts("0x");
            print_hex(va_arg(args, uint32_t), true);
        } else if (ch == '%') {
            k_putc('%');
        } else if (ch == '\0') { // the string has ended in the middle of a place
            break;
        }
    }

    va_end(args);
}
