/* The functions of `calls_asm.s` in C, for the build on the computer that runs the tests. */

int c_mix(int a, int b, int c);

extern int c_total;

int asm_number = 0x12345;
int (*asm_handler)(int a, int b, int c) = c_mix;

int asm_bump(int by)
{
    c_total += by;

    return c_total;
}

int asm_weigh(int a, int b, int c)
{
    return a + 2 * b + 4 * c;
}

int asm_call_c(int a, int b)
{
    return c_mix(a, b, 3) + 1;
}

int asm_call_ptr(int (*func)(int, int), int a, int b)
{
    return func(a, b);
}

int asm_keeps_regs(int (*func)(int), int x)
{
    return func(x);
}

long long asm_wide(int low, int high)
{
    return (long long) (((unsigned long long) (unsigned int) high << 32) | (unsigned int) low);
}

int asm_halves(long long value, int extra)
{
    unsigned int low = (unsigned int) value;
    unsigned int high = (unsigned int) ((unsigned long long) value >> 32);

    return (int) ((low ^ (high << 1)) + (unsigned int) extra);
}

signed char asm_low_byte(int x)
{
    return (signed char) x;
}
