/*
    64-bit numbers on a 32-bit machine.

    A `long long` takes two registers or two words of memory. The compiler
    adds the lower halves with `add` and the upper halves with `adc`, which
    adds the carry that the first addition has left in the flags. `sub` and
    `sbb` do the same with the borrow. The machine multiplies to 32 bits and
    does not divide, so the 64-bit multiplication, the divisions, and the
    shifts are functions of the runtime library.
*/

#include "out.h"

#define VALUE_COUNT 12

static const unsigned long long VALUES[VALUE_COUNT] = {
    0x0000000000000000ULL,
    0x0000000000000001ULL,
    0x0000000000000007ULL,
    0x00000000ffffffffULL,
    0x0000000100000000ULL,
    0x000000037fffffffULL,
    0x7fffffffffffffffULL,
    0x8000000000000000ULL,
    0xffffffffffffffffULL,
    0xfffffffffffffff9ULL,
    0x0123456789abcdefULL,
    0xfedcba9876543210ULL
};

#define COUNT_COUNT 8

static const int COUNTS[COUNT_COUNT] = { 0, 1, 4, 31, 32, 33, 60, 63 };

long long g_counter = 0x00000000fffffffeLL;

static unsigned long long add(unsigned long long a, unsigned long long b)
{
    return a + b;
}

static long long negate(long long a)
{
    return -a;
}

static int compare(long long a, long long b)
{
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }

    return 0;
}

static void head(const char *name, unsigned long long a, unsigned long long b)
{
    put_str(name);
    put_char(' ');
    put_hex64(a);
    put_char(' ');
    put_hex64(b);
    put_str(": ");
}

static void test_carry(void)
{
    /* The carry and the borrow between the halves. */
    show64("0xffffffff + 1", add(0xffffffffULL, 1));
    show64("0x100000000 - 1", 0x100000000ULL - add(0, 1));
    show64("-1 + 1", add(0xffffffffffffffffULL, 1));
    show64("0 - 1", add(0, 0) - add(0, 1));
    show64("negate(1)", negate(1));
    show64("negate(0x100000000)", negate(0x100000000LL));

    for (int i = 0; i < 4; ++i) {
        ++g_counter;
        show64("++g_counter", g_counter);
    }
    for (int i = 0; i < 4; ++i) {
        g_counter--;
    }
    show64("g_counter", g_counter);

    /* A 64-bit argument takes two words of the stack, and the caller removes
       both after the call. If it left one, this loop would use up the stack. */
    unsigned long long total = 0;
    for (int i = 0; i < 30000; ++i) {
        total = add(total, (unsigned long long) i << 20);
    }
    show64("loop of calls", total);

    g_counter += 0x7fffffff;
    g_counter -= 0x80000000LL;
    show64("g_counter", g_counter);
}

static void test_arithmetic(void)
{
    for (int i = 0; i < VALUE_COUNT; ++i) {
        for (int j = 0; j < VALUE_COUNT; ++j) {
            unsigned long long a = VALUES[i];
            unsigned long long b = VALUES[j];

            head("add sub mul", a, b);
            put_hex64(a + b);
            put_char(' ');
            put_hex64(a - b);
            put_char(' ');
            put_hex64(a * b);
            put_char('\n');

            head("and or xor", a, b);
            put_hex64(a & b);
            put_char(' ');
            put_hex64(a | b);
            put_char(' ');
            put_hex64(a ^ ~b);
            put_char('\n');

            head("compare", a, b);
            put_dec(compare((long long) a, (long long) b));
            put_char(' ');
            put_dec((a < b) + 2 * (a <= b) + 4 * (a > b) + 8 * (a >= b) + 16 * (a == b) + 32 * (a != b));
            put_char(' ');
            put_dec(((long long) a < (long long) b) + 2 * ((long long) a <= (long long) b) +
                    4 * ((long long) a > (long long) b) + 8 * ((long long) a >= (long long) b));
            put_char('\n');
        }
    }
}

static void test_division(void)
{
    for (int i = 0; i < VALUE_COUNT; ++i) {
        for (int j = 0; j < VALUE_COUNT; ++j) {
            unsigned long long a = VALUES[i];
            unsigned long long b = VALUES[j];

            if (b == 0) {
                continue;
            }

            head("udiv umod", a, b);
            put_hex64(a / b);
            put_char(' ');
            put_hex64(a % b);
            put_char('\n');

            /* The lowest negative number divided by -1 has no answer. */
            if (a == 0x8000000000000000ULL && b == 0xffffffffffffffffULL) {
                continue;
            }

            head("div mod", a, b);
            put_hex64((unsigned long long) ((long long) a / (long long) b));
            put_char(' ');
            put_hex64((unsigned long long) ((long long) a % (long long) b));
            put_char('\n');
        }
    }

    show64("wide / 10", 0x0123456789abcdefLL / 10);
    show64("wide % 10", 0x0123456789abcdefLL % 10);
    show64("-wide / 1000", -0x0123456789abcdefLL / 1000);
    show64("-wide % 1000", -0x0123456789abcdefLL % 1000);
}

static void test_shifts(void)
{
    for (int i = 0; i < VALUE_COUNT; ++i) {
        for (int j = 0; j < COUNT_COUNT; ++j) {
            unsigned long long a = VALUES[i];
            int count = COUNTS[j];

            head("shl shr sar", a, (unsigned long long) count);
            put_hex64(a << count);
            put_char(' ');
            put_hex64(a >> count);
            put_char(' ');
            put_hex64((unsigned long long) ((long long) a >> count));
            put_char('\n');
        }
    }

    unsigned long long wide = 0x8123456789abcdefULL;
    show64("wide << 4", wide << 4);
    show64("wide >> 4", wide >> 4);
    show64("wide >> 36", wide >> 36);
    show64("(long long) wide >> 36", (unsigned long long) ((long long) wide >> 36));
    show64("wide << 32", wide << 32);
}

static void test_conversions(void)
{
    int negative = -2;
    unsigned int positive = 0xfffffffe;
    signed char byte = -3;
    unsigned short half = 0xfffd;
    long long wide = 0x0123456789abcdefLL;

    show64("(long long) negative", (unsigned long long) (long long) negative);
    show64("(long long) positive", (unsigned long long) (long long) positive);
    show64("(long long) byte", (unsigned long long) (long long) byte);
    show64("(long long) half", (unsigned long long) (long long) half);
    show("(int) wide", (unsigned int) (int) wide);
    show("(int) (wide >> 32)", (unsigned int) (int) (wide >> 32));
    show("(signed char) wide", (unsigned int) (signed char) wide);
    show64("negative * wide", (unsigned long long) (negative * wide));
    show64("positive * wide", (unsigned long long) (positive * wide));
    show64("(long long) negative * negative", (unsigned long long) ((long long) negative * negative));
    show64("(unsigned long long) positive * positive", (unsigned long long) positive * positive);
    show("wide != 0", wide != 0);
    show("!wide", !wide);
    show("wide && negative", wide && negative);
    show("(wide & 0xf00000000) != 0", (wide & 0xf00000000LL) != 0);

    long long values[3] = { 1, -1, 0x100000000LL };
    long long total = 0;
    for (int i = 0; i < 3; ++i) {
        total += values[i] * (i + 1);
    }
    show64("total", (unsigned long long) total);
}

int main(void)
{
    test_carry();
    test_arithmetic();
    test_division();
    test_shifts();
    test_conversions();

    return 0;
}
