/*
    A program that checks the instruction `dot4` from the side of a process.

    `dot4` takes two words as four signed bytes each, multiplies the bytes in
    the same places, and adds the four products to a number. The chip does it
    in its dot-product unit, over several clocks. A program runs with paging
    on, and the timer interrupts it at any instruction, also at a `dot4`, and
    another process, which may use the unit too, runs in between. The result
    must be the same as without that.

    The program makes a child, so that two processes use the unit in turns.
    Both compute sums of products for words that follow from a start value,
    once with `dot4` and once in C with four multiplications, and they stop
    when enough timer interrupts have arrived.
*/

#include "user.h"

#include "check.h"

#define ROUNDS_PER_CHECK 50
#define DOT_TICKS 16 // timer interrupts during which a process computes

// A function of `dots_asm.s`: returns `sum` plus the four products of the signed bytes of `a` and `b`.
int dot4(unsigned int a, unsigned int b, int sum);

// The same sum with four multiplications of C.
static int add_products(unsigned int a, unsigned int b, int sum)
{
    for (int i = 0; i < 4; ++i) {
        sum += (signed char) (a >> (8 * i)) * (signed char) (b >> (8 * i));
    }

    return sum;
}

/*
    Returns the number that follows `number` in a series that looks random
    and never reaches 0 (the xorshift generator of Marsaglia).
*/
static unsigned int next_number(unsigned int number)
{
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;

    return number;
}

/*
    Computes sums of products with `dot4` and in C until the timer has
    interrupted the machine `ticks` times. Each sum is added to the sum
    before it, so one wrong product changes every sum after it. Returns 0 if
    the two sums were equal all the time.
*/
static int compute(unsigned int number, int ticks)
{
    int start = times(NULL);

    int sum = 0;
    int sum_in_c = 0;
    do {
        for (int i = 0; i < ROUNDS_PER_CHECK; ++i) {
            unsigned int a = number = next_number(number);
            unsigned int b = number = next_number(number);

            sum = dot4(a, b, sum);
            sum_in_c = add_products(a, b, sum_in_c);
            if (sum != sum_in_c) {
                return 1;
            }
        }
    } while (times(NULL) - start < ticks);

    return 0;
}

int main(void)
{
    // Sums that are known: 1*5 + 2*6 + 3*7 + 4*8 = 70, and four times (-128)*127 added to 24
    check(dot4(0x04030201, 0x08070605, 100) == 170, "dot4 adds four products to a number");
    check(dot4(0x80808080, 0x7f7f7f7f, 24) == -65000, "the bytes are signed");

    tms_t before;
    int start = times(&before);

    int pid = fork();
    if (pid == 0) {
        exit(compute(0x2545f491, DOT_TICKS));
    }

    int result = compute(0x9e3779b9, DOT_TICKS);

    // Each of the two has run for less time than has passed, so the other one ran in between
    tms_t after;
    int passed = times(&after) - start;

    int status = -1;
    int waited_pid = waitpid(pid, &status, 0);

    check(result == 0, "the timer has interrupted the sums of dot4, and they are right");
    check(pid > 0 && waited_pid == pid && status == 0, "a child has computed its own sums at the same time");
    check(after.utime > before.utime && (int) (after.utime - before.utime) < passed, "the two processes took turns in the middle of their sums");

    return get_result();
}
