/*
    Calls between C and assembly, in both directions.

    The calling convention of the machine:

    * The caller pushes the arguments, the last one first, so the first
      argument is on the top of the stack when the function starts. Each
      argument takes one word of 4 bytes. A 64-bit argument takes two, its
      lower half at the lower address.
    * `call` puts the return address in `lr`. A function that calls another
      one pushes `lr` first.
    * The result is in `r0`. A 64-bit result has its upper half in `r1`. A
      function that returns a `char` or a `short` returns it extended to 32
      bits.
    * The caller removes the arguments after the call.
    * A function may change `r0`-`r3`. It keeps `r4`-`r7`, `fp`, and `sp`.

    The functions with the names `asm_...` are written in assembly in
    `calls_asm.s`. `calls_host.c` has the same functions in C for the build on
    the computer that runs the tests.
*/

#include "out.h"

int asm_weigh(int a, int b, int c);
int asm_call_c(int a, int b);
int asm_call_ptr(int (*func)(int, int), int a, int b);
int asm_keeps_regs(int (*func)(int), int x);
long long asm_wide(int low, int high);
int asm_halves(long long value, int extra);
signed char asm_low_byte(int x);

/* Variables that the assembly file defines: a number, and a pointer that the
   linker fills with the address of the C function `c_mix`. */
extern int asm_number;
extern int (*asm_handler)(int a, int b, int c);

int asm_bump(int by);

/* Changed from assembly by its name. */
int c_total = 1000;

/* Called from assembly by its name. */
int c_mix(int a, int b, int c)
{
    return a * 100 + b * 10 + c;
}

static int add(int a, int b)
{
    return a + b;
}

static int sub(int a, int b)
{
    return a - b;
}

static int mul(int a, int b)
{
    return a * b;
}

static int twice(int x)
{
    return 2 * x;
}

static int fact(int n)
{
    return n <= 1 ? 1 : n * fact(n - 1);
}

static int fib(int n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static int many(int a, int b, int c, int d, int e, int f, int g, int h)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h;
}

typedef int (*op_t)(int, int);

/* The linker fills this table with the addresses of the functions. */
static const op_t OPS[3] = { add, sub, mul };

static op_t pick(int index)
{
    return OPS[index];
}

int main(void)
{
    /* C calls assembly. */
    show("asm_weigh", asm_weigh(1, 2, 3));
    show64("asm_wide", asm_wide(0x89abcdef, 0x01234567));
    show("asm_halves", asm_halves(0x0123456789abcdefLL, 5));
    show("asm_low_byte", asm_low_byte(0x1234ff80));
    show("asm_low_byte", asm_low_byte(0x1234ff7f));

    /* Assembly calls C: by name, and through a pointer. */
    show("asm_call_c", asm_call_c(4, 5));
    show("asm_call_ptr add", asm_call_ptr(add, 30, 12));
    show("asm_call_ptr sub", asm_call_ptr(sub, 30, 12));
    show("asm_keeps_regs", asm_keeps_regs(twice, 21));

    /* Variables of the assembly file, and a C variable that it changes. */
    show("asm_number", asm_number);
    asm_number += 1;
    show("asm_number", asm_number);
    show("asm_handler", asm_handler(7, 8, 9));
    show("asm_handler == c_mix", asm_handler == c_mix);
    show("asm_bump", asm_bump(5));
    show("c_total", c_total);

    /* C calls C through a pointer. */
    for (int i = 0; i < 3; ++i) {
        show("table", OPS[i](7, 5));
        show("picked", pick(i)(9, 4));
    }
    op_t op = mul;
    show("local pointer", op(6, 7));
    op = pick(1);
    show("local pointer", op(6, 7));

    /* Calls as arguments of calls, a function that calls itself, and more
       arguments than there are registers. */
    show("nested", add(mul(2, 3), sub(10, add(1, 2))));
    show("fact", fact(10));
    show("fib", fib(15));
    show("many", many(1, 2, 3, 4, 5, 6, 7, 8));
    show("many nested", many(add(1, 1), 2, mul(3, 3), 4, fact(5), 6, fib(7), 8));

    /* The caller removes the arguments after every call. If it did not, this
       loop would move the stack pointer through all of memory. */
    int total = 0;
    for (int i = 0; i < 10000; ++i) {
        total = add(total, many(i, 1, 1, 1, 1, 1, 1, 1));
    }
    show("loop of calls", total);

    return 0;
}
