/*
    The operations of C on 32-bit numbers, and the statements that choose
    what runs next.

    The machine has no instruction for a division: `/` and `%` are calls to
    the runtime library. A comparison sets the flags, and then either a
    conditional jump uses them (`if`, a loop) or `setcc` turns them into a 1
    or a 0 (a comparison used as a number).
*/

#include "out.h"

#define VALUE_COUNT 14

static const unsigned int VALUES[VALUE_COUNT] = {
    0x00000000, 0x00000001, 0x00000002, 0x00000007, 0x0000000a, 0x00007fff, 0x00008000,
    0x0000ffff, 0x00010000, 0x12345678, 0x7fffffff, 0x80000000, 0xfffffff9, 0xffffffff
};

static void head(const char *name, unsigned int a, unsigned int b)
{
    put_str(name);
    put_char(' ');
    put_hex(a);
    put_char(' ');
    put_hex(b);
    put_str(":");
}

static void item(unsigned int value)
{
    put_char(' ');
    put_hex(value);
}

/* Every comparison as a number: one bit of the result for each. */
static unsigned int compare_values(int a, int b)
{
    unsigned int ua = (unsigned int) a;
    unsigned int ub = (unsigned int) b;

    return (a == b) | (a != b) << 1 | (a < b) << 2 | (a >= b) << 3 | (a <= b) << 4 | (a > b) << 5 |
           (ua < ub) << 6 | (ua >= ub) << 7 | (ua <= ub) << 8 | (ua > ub) << 9;
}

/* The same comparisons as jumps. */
static unsigned int compare_jumps(int a, int b)
{
    unsigned int ua = (unsigned int) a;
    unsigned int ub = (unsigned int) b;
    unsigned int result = 0;

    if (a == b) result |= 1 << 0;
    if (a != b) result |= 1 << 1;
    if (a < b) result |= 1 << 2;
    if (a >= b) result |= 1 << 3;
    if (a <= b) result |= 1 << 4;
    if (a > b) result |= 1 << 5;
    if (ua < ub) result |= 1 << 6;
    if (ua >= ub) result |= 1 << 7;
    if (ua <= ub) result |= 1 << 8;
    if (ua > ub) result |= 1 << 9;

    return result;
}

static void test_arithmetic(void)
{
    for (int i = 0; i < VALUE_COUNT; ++i) {
        for (int j = 0; j < VALUE_COUNT; ++j) {
            unsigned int a = VALUES[i];
            unsigned int b = VALUES[j];

            head("add sub mul and or xor", a, b);
            item(a + b);
            item(a - b);
            item(a * b);
            item(a & b);
            item(a | b);
            item(a ^ b);
            put_char('\n');

            head("compare", a, b);
            item(compare_values((int) a, (int) b));
            item(compare_jumps((int) a, (int) b));
            put_char('\n');

            if (b == 0) {
                continue;
            }

            head("udiv umod", a, b);
            item(a / b);
            item(a % b);
            put_char('\n');

            /* The lowest negative number divided by -1 has no answer. */
            if (a == 0x80000000 && b == 0xffffffff) {
                continue;
            }

            head("div mod", a, b);
            item((unsigned int) ((int) a / (int) b));
            item((unsigned int) ((int) a % (int) b));
            put_char('\n');
        }
    }
}

static void test_shifts(void)
{
    for (int i = 0; i < VALUE_COUNT; ++i) {
        unsigned int a = VALUES[i];

        for (int count = 0; count < 32; count += 5) {
            head("shl shr sar", a, (unsigned int) count);
            item(a << count);
            item(a >> count);
            item((unsigned int) ((int) a >> count));
            put_char('\n');
        }

        head("not neg", a, 0);
        item(~a);
        item(0u - a);
        item(!a);
        put_char('\n');
    }
}

/* Numbers that do not fit the 16-bit field of an instruction. */
static void test_constants(void)
{
    int number = 1000;

    show("number + 0x7fff", number + 0x7fff);
    show("number + 0x8000", number + 0x8000);
    show("number - 0x8000", number - 0x8000);
    show("number - 0x8001", number - 0x8001);
    show("number + -0x8000", number + -0x8000);
    show("number + -0x8001", number + -0x8001);
    show("number & ~0x8000", number & ~0x8000);
    show("number | -0x8001", number | -0x8001);
    show("number > -0x8001", number > -0x8001);
    show("number == -0x8001", number - 0x8001 - 1000 == -0x8001);
    show("number + 0x12345678", number + 0x12345678);
    show("number * 100000", number * 100000);
    show("number & 0xffff", number & 0xffff);
    show("number | 0x80000000", number | 0x80000000);
    show("number ^ 0xffff0000", number ^ 0xffff0000);
    show("number == 1000", number == 1000);
    show("number < 0x10000", number < 0x10000);
    show("number > -100000", number > -100000);
    show("(unsigned) number < 0xffff8000", (unsigned int) number < 0xffff8000);
    show("number / 7", number / 7);
    show("number % 7", number % 7);
    show("-number / 7", -number / 7);
    show("-number % 7", -number % 7);
    show("number * 3 / 2 - 1", number * 3 / 2 - 1);
}

/*
    C does not say what a division by zero gives, and the machine has no
    exception for it. The runtime library answers with a quotient of all ones
    and with the dividend as the remainder. The build on the computer that
    runs the tests prints the same numbers without dividing.
*/
static void test_division_by_zero(void)
{
#ifdef __asc2__
    unsigned int zero = VALUES[0];
    unsigned long long wide = 0x0123456789abcdefULL;

    show("7u / 0", 7u / zero);
    show("7u % 0", 7u % zero);
    show("-7 / 0", -7 / (int) zero);
    show("-7 % 0", -7 % (int) zero);
    show64("wide / 0", wide / zero);
    show64("wide % 0", wide % zero);
#else
    show("7u / 0", 0xffffffff);
    show("7u % 0", 7);
    show("-7 / 0", 1);
    show("-7 % 0", (unsigned int) -7);
    show64("wide / 0", 0xffffffffffffffffULL);
    show64("wide % 0", 0x0123456789abcdefULL);
#endif
}

/* More values in use at once than the compiler has registers for. */
static int wide_expression(int a, int b, int c, int d, int e, int f, int g, int h)
{
    return ((a + b) * (c - d) + (e ^ f) * (g | h)) - ((a * e + b * f) * (c * g - d * h)) +
           (((a - h) * (b + g)) ^ ((c - f) * (d + e)));
}

static int square(int x)
{
    return x * x;
}

static void test_expressions(void)
{
    int a = 3;
    int b = -4;
    int c = 5;

    show("wide_expression", wide_expression(1, 2, 3, 4, 5, 6, 7, 8));
    show("wide_expression", wide_expression(-9, 8, -7, 6, -5, 4, -3, 2));
    show("calls in an expression", square(a) + square(b) * square(c) - square(a + b) * (c + square(2)));

    a += b;
    b -= c;
    c *= a;
    show("a", a);
    show("b", b);
    show("c", c);
    c <<= 3;
    b >>= 1;
    a ^= c;
    a |= 0x100;
    a &= ~0x3;
    show("a", a);
    show("b", b);
    show("c", c);
    c /= 7;
    b %= 5;
    show("b", b);
    show("c", c);

    show("a++", a++);
    show("++a", ++a);
    show("a--", a--);
    show("--a", --a);
    show("comma", (a = 9, b = a + 1, a * b));
    show("ternary", a > b ? a : b);
    show("ternary", a < b ? a - 1 : b + 1);

    /* `&&` and `||` do not run their right side when the left side decides. */
    int calls = 0;
    show("0 && ++calls", 0 && ++calls);
    show("1 || ++calls", 1 || ++calls);
    show("1 && ++calls", 1 && ++calls);
    show("0 || ++calls", 0 || ++calls);
    show("calls", calls);
    show("a && !b", a && !b);
    show("!a || !!b", !a || !!b);
    show("(a > 5 && b > 5) || c == 0", (a > 5 && b > 5) || c == 0);
}

static int classify(int x)
{
    switch (x) {
        case 0:
            return 100;
        case 1:
        case 2:
            return 200;
        case 3:
            x += 10;
            /* Falls through. */
        case 4:
            return x + 300;
        case 1000:
            return 400;
        case -5:
            return 500;
        case 0x12345:
            return 600;
        default:
            return -1;
    }
}

static int collatz_steps(unsigned int n)
{
    int steps = 0;

    while (n != 1) {
        n = n % 2 == 0 ? n / 2 : 3 * n + 1;
        ++steps;
    }

    return steps;
}

/* An array with a size known only when the program runs. The stack pointer
   moves down by the size, rounded up to whole words, and the other local
   variables stay where they were. */
static int fill_scratch(int size, int seed)
{
    int before = seed;
    char scratch[size];
    int after = seed * 3;
    int sum = 0;

    for (int i = 0; i < size; ++i) {
        scratch[i] = (char) (seed + i);
    }
    for (int i = 0; i < size; ++i) {
        sum += square(scratch[i]) + i;
    }

    return sum + before + after + (int) sizeof(scratch);
}

static void test_control(void)
{
    static const int CASES[] = { 0, 1, 2, 3, 4, 5, 1000, -5, 0x12345, 0x12346 };

    for (unsigned int i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i) {
        show("classify", classify(CASES[i]));
    }

    int sum = 0;
    for (int i = 0; i < 100; ++i) {
        if (i % 3 == 0) {
            continue;
        }
        if (i > 50) {
            break;
        }
        sum += i;
    }
    show("for with continue and break", sum);

    int count = 10;
    sum = 0;
    do {
        sum += count * count;
    } while (--count > 0);
    show("do while", sum);

    show("collatz_steps(27)", collatz_steps(27));

    /* A loop of loops left with `goto`. */
    int row = 0;
    int column = 0;
    for (row = 0; row < 10; ++row) {
        for (column = 0; column < 10; ++column) {
            if (row * column == 42) {
                goto found;
            }
        }
    }
found:
    show("row", row);
    show("column", column);

    /* A jump to an address kept in a variable, an extension of GNU C. */
    void *target = &&second;
    int path = 0;
    goto *target;
first:
    path += 1;
    goto finish;
second:
    path += 20;
    target = &&first;
    goto *target;
finish:
    show("path", path);

    /* A large array on the stack. */
    int numbers[500];
    for (int i = 0; i < 500; ++i) {
        numbers[i] = i * i;
    }
    sum = 0;
    for (int i = 499; i >= 0; i -= 7) {
        sum += numbers[i];
    }
    show("large array", sum);

    for (int size = 1; size < 70; size += 11) {
        show("fill_scratch", fill_scratch(size, size * 5));
    }

    /* The room of such an array is given back at the end of its block. If
       it were not, this loop would use up the stack. */
    sum = 0;
    for (int round = 0; round < 3000; ++round) {
        int size = round % 61 + 100;
        char scratch[size];

        scratch[0] = (char) round;
        scratch[size - 1] = (char) (round >> 4);
        sum += square(scratch[0]) + scratch[size - 1] + (int) sizeof(scratch);
    }
    show("arrays of a size found at run time", sum);
}

int main(void)
{
    test_arithmetic();
    test_shifts();
    test_constants();
    test_division_by_zero();
    test_expressions();
    test_control();

    return 0;
}
