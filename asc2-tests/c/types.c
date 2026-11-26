/*
    The types of C on the machine: bytes and 16-bit numbers with and without
    a sign, conversions between them, global variables, pointers, arrays, and
    bit-fields.

    A value shorter than 32 bits lives in memory at its own size and in a
    register at 32 bits. A load extends it: with copies of its sign bit
    (`lrb`, `lrh`) or with zeros (`lrbu`, `lrhu`). A store keeps the lower
    bits (`srb`, `srh`).
*/

#include "out.h"

/* Global variables with a starting value are in the data of the program. */
signed char g_sbyte = -5;
unsigned char g_ubyte = 250;
short g_short = -1234;
unsigned short g_ushort = 65000;
int g_int = 0x12345678;
long long g_wide = 0x1122334455667788LL;

/* Global variables without one are zeros when `main` starts. */
int g_zero;
int g_zeros[8];

int g_values[6] = { 10, 20, 30, 40, 50, 60 };

/* The linker puts an address into each of these. The second one is an
   address inside an array. */
int *g_first = g_values;
int *g_fourth = &g_values[3];
const char *g_names[3] = { "one", "two", "three" };

/* An array longer than 64 KB. An instruction holds 16 bits of an address, so
   an address takes two instructions, and the parts far from the start of
   this array differ from it in the upper half too. */
static char g_far[0x12000];
static char *g_far_end = &g_far[0x11fff];

typedef struct switches
{
    char before;
    _Bool on;
    char after;
} switches_t;

switches_t g_switches = { -1, 0, -1 };

typedef enum color
{
    COLOR_RED,
    COLOR_GREEN = 5,
    COLOR_BLUE
} color_t;

typedef union word
{
    unsigned int word;
    unsigned short halves[2];
    unsigned char bytes[4];
} word_t;

typedef struct flags
{
    unsigned int low : 3;
    unsigned int middle : 5;
    signed int high : 4;
    unsigned int rest : 20;
} flags_t;

static signed char to_sbyte(int x)
{
    return (signed char) x;
}

static unsigned char to_ubyte(int x)
{
    return (unsigned char) x;
}

static short to_short(int x)
{
    return (short) x;
}

static unsigned short to_ushort(int x)
{
    return (unsigned short) x;
}

static int from_sbyte(signed char x)
{
    return x;
}

static int from_ushort(unsigned short x)
{
    return x;
}

static int next_number(void)
{
    /* A `static` variable in a function keeps its value between calls. */
    static int number = 100;

    return number++;
}

static int length(const char *str)
{
    const char *end = str;

    while (*end != '\0') {
        ++end;
    }

    return (int) (end - str);
}

static void test_sizes(void)
{
    show("sizeof char", sizeof(char));
    show("sizeof short", sizeof(short));
    show("sizeof int", sizeof(int));
    show("sizeof long long", sizeof(long long));
    show("sizeof word_t", sizeof(word_t));
    show("sizeof flags_t", sizeof(flags_t));
    show("sizeof g_values", sizeof(g_values));
}

static void test_bytes(void)
{
    /* A signed byte: loaded with its sign, stored without the upper bits. */
    show("g_sbyte", g_sbyte);
    show("g_ubyte", g_ubyte);
    show("g_sbyte + g_ubyte", g_sbyte + g_ubyte);
    show("g_sbyte < 0", g_sbyte < 0);
    show("g_ubyte > 0", g_ubyte > 0);

    g_sbyte = (signed char) 200;
    show("g_sbyte = 200", g_sbyte);
    g_ubyte = (unsigned char) -3;
    show("g_ubyte = -3", g_ubyte);

    signed char sbyte = 127;
    ++sbyte;
    show("127 + 1 as a signed byte", sbyte);
    unsigned char ubyte = 255;
    ++ubyte;
    show("255 + 1 as an unsigned byte", ubyte);
    sbyte = -128;
    --sbyte;
    show("-128 - 1 as a signed byte", sbyte);

    show("to_sbyte(0x17f)", to_sbyte(0x17f));
    show("to_sbyte(0x180)", to_sbyte(0x180));
    show("to_ubyte(0x180)", to_ubyte(0x180));
    show("to_ubyte(-1)", to_ubyte(-1));
    show("from_sbyte(0x80)", from_sbyte((signed char) 0x80));
    show("from_sbyte(0x7f)", from_sbyte(0x7f));

    signed char samples[5] = { -100, -1, 0, 1, 100 };
    int sum = 0;
    for (int i = 0; i < 5; ++i) {
        sum += samples[i] * (i + 1);
    }
    show("sum of signed bytes", sum);

    char text[] = "bytes";
    text[0] = 'B';
    show("text[0]", text[0]);
    show("length(text)", length(text));
    show("sizeof text", sizeof(text));
}

static void test_shorts(void)
{
    show("g_short", g_short);
    show("g_ushort", g_ushort);
    show("g_short + g_ushort", g_short + g_ushort);

    g_short = (short) 40000;
    show("g_short = 40000", g_short);
    g_ushort = (unsigned short) -2;
    show("g_ushort = -2", g_ushort);

    show("to_short(0x18000)", to_short(0x18000));
    show("to_short(0x17fff)", to_short(0x17fff));
    show("to_ushort(0x18000)", to_ushort(0x18000));
    show("from_ushort(0xffff)", from_ushort(0xffff));

    short pair[2] = { -2, 3 };
    show("pair[0] * pair[1]", pair[0] * pair[1]);
}

static void test_conversions(void)
{
    int number = 0x8765fedc;
    unsigned int positive = 0x8765fedc;

    show("(signed char) number", (signed char) number);
    show("(unsigned char) number", (unsigned char) number);
    show("(short) number", (short) number);
    show("(unsigned short) number", (unsigned short) number);
    show("(int)(signed char)(short) number", (int) (signed char) (short) number);
    show("(unsigned short)(signed char) number", (unsigned short) (signed char) number);

    /* A comparison of a signed and an unsigned number is unsigned. */
    show("number < 0", number < 0);
    show("positive > 0", positive > 0);
    show("-1 < 1u", -1 < 1u);
    show("number >> 4", number >> 4);
    show("positive >> 4", positive >> 4);

    show64("(long long) number", (long long) number);
    show64("(long long) positive", (long long) positive);
    show64("(long long) g_sbyte", (long long) g_sbyte);
    show("(int) g_wide", (int) g_wide);
    show("(short) g_wide", (short) g_wide);

    _Bool truth = 0x100;
    show("(_Bool) 0x100", truth);
    truth = !truth;
    show("!truth", truth);
    show("COLOR_BLUE", COLOR_BLUE);
    color_t color = COLOR_GREEN;
    show("color", color);
}

static void test_globals(void)
{
    show("g_int", g_int);
    show64("g_wide", g_wide);
    show("g_zero", g_zero);
    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        sum |= g_zeros[i];
    }
    show("g_zeros", sum);

    g_int += 1;
    g_zero = g_int * 2;
    g_zeros[7] = g_zero + 1;
    g_wide += 0x100000000LL;
    show("g_int", g_int);
    show("g_zero", g_zero);
    show("g_zeros[7]", g_zeros[7]);
    show64("g_wide", g_wide);

    show("next_number", next_number());
    show("next_number", next_number());
    show("next_number", next_number());

    g_far[0] = 1;
    g_far[0x7ffe] = 2;
    g_far[0x8001] = 3;
    g_far[0xffff] = 4;
    g_far[0x10000] = 5;
    g_far[0x11ffe] = 6;
    *g_far_end = 7;
    int index = 0x8001;
    show("g_far near", g_far[0] + g_far[0x7ffe] * 10 + g_far[index] * 100);
    index = 0x10000;
    show("g_far far", g_far[0xffff] + g_far[index] * 10 + g_far[0x11ffe] * 100 + g_far[0x11fff] * 1000);
    show("g_far_end - g_far", g_far_end - g_far);
    show("&g_far[0x11000] - &g_far[0x7000]", &g_far[0x11000] - &g_far[0x7000]);

    g_switches.on = 5;
    show("g_switches.on", g_switches.on);
    show("g_switches.on + before", g_switches.on + g_switches.before);
    g_switches.on = !g_switches.on;
    show("g_switches.on", g_switches.on);
    show("g_switches.after", g_switches.after);
}

static void test_pointers(void)
{
    show("*g_first", *g_first);
    show("*g_fourth", *g_fourth);
    show("g_fourth[-1]", g_fourth[-1]);
    show("g_fourth - g_first", g_fourth - g_first);

    int *cursor = g_values;
    cursor += 2;
    show("*cursor", *cursor);
    show("*cursor++", *cursor++);
    show("*++cursor", *++cursor);
    show("cursor - g_values", cursor - g_values);
    show("cursor > g_fourth", cursor > g_fourth);

    int **indirect = &g_fourth;
    **indirect = 41;
    show("g_values[3]", g_values[3]);
    *indirect = &g_values[5];
    show("*g_fourth", *g_fourth);

    for (int i = 0; i < 3; ++i) {
        put_str(g_names[i]);
        put_char(' ');
        put_dec(length(g_names[i]));
        put_char('\n');
    }
    show("g_names[2][1]", g_names[2][1]);

    int table[3][4];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            table[i][j] = i * 10 + j;
        }
    }
    show("table[2][3]", table[2][3]);
    show("table[1][0]", *(*(table + 1)));
    show("sizeof table[0]", sizeof(table[0]));

    /* The lowest byte of a word has the lowest address. */
    word_t word;
    word.word = 0x11223344;
    show("bytes[0]", word.bytes[0]);
    show("bytes[3]", word.bytes[3]);
    show("halves[0]", word.halves[0]);
    word.bytes[1] = 0xaa;
    word.halves[1] = 0xbbcc;
    show("word", word.word);
}

static void test_bit_fields(void)
{
    flags_t flags;
    word_t *raw = (word_t *) &flags;

    raw->word = 0;
    flags.low = 5;
    flags.middle = 19;
    flags.high = -3;
    flags.rest = 0xabcde;
    show("raw", raw->word);
    show("low", flags.low);
    show("middle", flags.middle);
    show("high", flags.high);
    show("rest", flags.rest);

    flags.low += 4;
    flags.high = 7;
    ++flags.high;
    show("low after + 4", flags.low);
    show("high after 7 + 1", flags.high);
    show("raw", raw->word);
}

int main(void)
{
    test_sizes();
    test_bytes();
    test_shorts();
    test_conversions();
    test_globals();
    test_pointers();
    test_bit_fields();

    return 0;
}
