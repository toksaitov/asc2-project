/*
    64-bit numbers for the Almurut II C compiler.

    A 64-bit number is two words of the machine. The compiler adds,
    subtracts, and compares such numbers by itself and calls these functions
    for the rest: division, the remainder, the 64-bit product of two words,
    and a shift by a count that is not a constant. A C program never names
    them.

    Division by zero gives a quotient of all ones and the dividend as the
    remainder, as for 32-bit numbers.
*/

typedef unsigned int u32_t;
typedef unsigned long long u64_t;

/* The two halves of a 64-bit number as the machine stores them: the low one first. */
typedef union llong_parts
{
    u64_t whole;
    struct
    {
        u32_t low;
        u32_t high;
    } half;
} llong_parts_t;

/* Division */

/* Long division in base 2, as `division.c` explains it, on 64-bit numbers. */
static u64_t udivmod64(u64_t num, u64_t den, u64_t *rem)
{
    u64_t quot = 0;
    u64_t bit = 1;

    if (den == 0) {
        *rem = num;
        return 0xffffffffffffffffULL;
    }

    while (den < num && !(den & 0x8000000000000000ULL)) {
        den <<= 1;
        bit <<= 1;
    }
    while (bit) {
        if (num >= den) {
            num -= den;
            quot |= bit;
        }
        den >>= 1;
        bit >>= 1;
    }

    *rem = num;
    return quot;
}

unsigned long long __udivdi3(unsigned long long num, unsigned long long den)
{
    u64_t rem;

    return udivmod64(num, den, &rem);
}

unsigned long long __umoddi3(unsigned long long num, unsigned long long den)
{
    u64_t rem;

    udivmod64(num, den, &rem);
    return rem;
}

long long __divdi3(long long num, long long den)
{
    u64_t rem;
    u64_t quot = udivmod64(num < 0 ? -(u64_t) num : (u64_t) num, den < 0 ? -(u64_t) den : (u64_t) den, &rem);

    return (num < 0) != (den < 0) ? -quot : quot;
}

long long __moddi3(long long num, long long den)
{
    u64_t rem;

    udivmod64(num < 0 ? -(u64_t) num : (u64_t) num, den < 0 ? -(u64_t) den : (u64_t) den, &rem);
    return num < 0 ? -rem : rem;
}

/* Multiplication */

/*
    The 64-bit product of two 32-bit numbers. `mul` gives the low 32 bits
    of a product, so each number is split into two 16-bit halves, whose
    four products fit into 32 bits each:

        a * b = (ah * bh << 32) + ((ah * bl + al * bh) << 16) + al * bl
*/
unsigned long long __umulsidi3(unsigned a, unsigned b)
{
    u32_t al = a & 0xffff;
    u32_t ah = a >> 16;
    u32_t bl = b & 0xffff;
    u32_t bh = b >> 16;

    u32_t low = al * bl;
    u32_t mid1 = ah * bl;
    u32_t mid2 = al * bh;
    u32_t high = ah * bh;

    /* the middle column of the sum and what it carries into the high half */
    u32_t mid = (low >> 16) + (mid1 & 0xffff) + (mid2 & 0xffff);

    llong_parts_t res;
    res.half.low = (low & 0xffff) | (mid << 16);
    res.half.high = high + (mid1 >> 16) + (mid2 >> 16) + (mid >> 16);
    return res.whole;
}

/* Shifts */

/* A 64-bit shift by a count that the compiler does not know: the bits that
   leave one half enter the other. The count is from 0 to 63. */

long long __ashldi3(long long a, int count)
{
    llong_parts_t val;
    val.whole = a;

    if (count >= 32) {
        val.half.high = val.half.low << (count - 32);
        val.half.low = 0;
    } else if (count != 0) {
        val.half.high = (val.half.high << count) | (val.half.low >> (32 - count));
        val.half.low <<= count;
    }

    return val.whole;
}

unsigned long long __lshrdi3(unsigned long long a, int count)
{
    llong_parts_t val;
    val.whole = a;

    if (count >= 32) {
        val.half.low = val.half.high >> (count - 32);
        val.half.high = 0;
    } else if (count != 0) {
        val.half.low = (val.half.low >> count) | (val.half.high << (32 - count));
        val.half.high >>= count;
    }

    return val.whole;
}

long long __ashrdi3(long long a, int count)
{
    llong_parts_t val;
    val.whole = a;

    if (count >= 32) {
        val.half.low = (int) val.half.high >> (count - 32);
        val.half.high = (int) val.half.high >> 31;
    } else if (count != 0) {
        val.half.low = (val.half.low >> count) | (val.half.high << (32 - count));
        val.half.high = (int) val.half.high >> count;
    }

    return val.whole;
}
