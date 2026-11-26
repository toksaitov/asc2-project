/*
    Division for the Almurut II C compiler.

    The machine has no instruction that divides. The compiler calls these
    functions for a division and for a remainder of 32-bit numbers. A C
    program never names them.

    Division by zero gives a quotient of all ones and the dividend as the
    remainder; the machine has no exception for it.
*/

typedef unsigned int u32_t;

/*
    Long division in base 2, as on paper. The divisor is first shifted left
    until it lines up with the dividend. Then, one bit of the quotient at a
    time, the divisor is subtracted where it fits and shifted back right.
    What is left of the dividend is the remainder.
*/
static u32_t udivmod32(u32_t num, u32_t den, u32_t *rem)
{
    u32_t quot = 0;
    u32_t bit = 1;

    if (den == 0) {
        *rem = num;
        return 0xffffffff;
    }

    while (den < num && !(den & 0x80000000)) {
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

unsigned __udivsi3(unsigned num, unsigned den)
{
    u32_t rem;

    return udivmod32(num, den, &rem);
}

unsigned __umodsi3(unsigned num, unsigned den)
{
    u32_t rem;

    udivmod32(num, den, &rem);
    return rem;
}

/* A signed division divides the sizes of the numbers and then gives the
   quotient the sign of C: negative if the signs of the operands differ. */
int __divsi3(int num, int den)
{
    u32_t rem;
    u32_t quot = udivmod32(num < 0 ? -(u32_t) num : (u32_t) num, den < 0 ? -(u32_t) den : (u32_t) den, &rem);

    return (num < 0) != (den < 0) ? -quot : quot;
}

/* The remainder has the sign of the dividend. */
int __modsi3(int num, int den)
{
    u32_t rem;

    udivmod32(num < 0 ? -(u32_t) num : (u32_t) num, den < 0 ? -(u32_t) den : (u32_t) den, &rem);
    return num < 0 ? -rem : rem;
}
