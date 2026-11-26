/*
    Memory at an address that the program names as a number.

    This is how C reaches the registers of a device: a number is turned into
    a pointer, and a read or a write through it becomes a load or a store at
    that address. The compiler must not treat such a place as the constant
    itself.

    The test uses words of RAM that nothing else occupies. The build on the
    computer that runs the tests has no such addresses and uses arrays.

    `LOW` is the same kind of place at an address small enough for the 16-bit
    field of an instruction. While paging is off, the addresses from 0 are
    the physical memory, so 0x7000 is RAM too.
*/

#include "out.h"

#ifdef __asc2__
#define WORDS ((volatile int *) 0x80020000)
#define BYTES ((volatile unsigned char *) 0x80020010)
#define LOW   ((volatile int *) 0x7000)
#else
static volatile int g_words[4];
static volatile unsigned char g_bytes[4];
static volatile int g_low[4];
#define WORDS g_words
#define BYTES g_bytes
#define LOW   g_low
#endif

static int sum3(int a, int b, int c)
{
    return a + 10 * b + 100 * c;
}

int main(void)
{
    WORDS[0] = 5;
    WORDS[1] = WORDS[0] + 1;
    WORDS[2] = -WORDS[1];
    WORDS[3] = 0x12345678;

    /* As an operand, as an argument, and in a comparison. */
    show("WORDS[0] + 3", WORDS[0] + 3);
    show("3 - WORDS[1]", 3 - WORDS[1]);
    show("WORDS[1] * WORDS[2]", WORDS[1] * WORDS[2]);
    show("sum3", sum3(WORDS[0], WORDS[1], WORDS[2]));
    show("WORDS[0] == 5", WORDS[0] == 5);
    show("WORDS[2] < 0", WORDS[2] < 0);
    show("WORDS[3] >> 8", WORDS[3] >> 8);

    WORDS[0] += 10;
    WORDS[1]++;
    WORDS[3] &= 0xffff;
    show("WORDS[0]", WORDS[0]);
    show("WORDS[1]", WORDS[1]);
    show("WORDS[3]", WORDS[3]);

    BYTES[0] = 0xff;
    BYTES[1] = (unsigned char) (BYTES[0] + 2);
    BYTES[2] = 0x80;
    show("BYTES[0]", BYTES[0]);
    show("BYTES[1]", BYTES[1]);
    show("(signed char) BYTES[2]", (signed char) BYTES[2]);
    show("sum3 of bytes", sum3(BYTES[0], BYTES[1], BYTES[2]));

    LOW[0] = 7;
    LOW[1] = LOW[0] * 2;
    LOW[2] = 0x7000;
    show("LOW[0] + 3", LOW[0] + 3);
    show("100 - LOW[1]", 100 - LOW[1]);
    show("LOW[0] == 7", LOW[0] == 7);
    show("LOW[2] == 0x7000", LOW[2] == 0x7000);
    show("sum3 of low words", sum3(LOW[0], LOW[1], LOW[2]));
    LOW[1] |= 0x100;
    show("LOW[1]", LOW[1]);

    /* A wait for a bit of a status register looks like this. */
    int rounds = 0;
    BYTES[3] = 0;
    while ((BYTES[3] & 0x8) == 0) {
        BYTES[3] = (unsigned char) (BYTES[3] + 3);
        ++rounds;
    }
    show("rounds", rounds);

    return 0;
}
