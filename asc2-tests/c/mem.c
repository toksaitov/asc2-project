/*
    The memory functions of the runtime library: `memset`, `memcpy`, and
    `memmove`.

    The compiler calls them by itself: `memmove` to copy a structure, and
    `memset` to clear a local array or structure that has a starting value
    for some of its parts only. A program may call them too.

    They move whole words where the addresses allow it and single bytes
    elsewhere, so the test tries every position of an address within a word
    together with sizes around a few words. The bytes on both sides of an
    area must stay as they were. `memset` uses the lowest byte of its value.
*/

#include "out.h"

#ifdef __asc2__
void *memset(void *dst, int val, unsigned int size);
void *memcpy(void *dst, const void *src, unsigned int size);
void *memmove(void *dst, const void *src, unsigned int size);
#else
#include <string.h>
#endif

#define AREA_SIZE 64
#define MAX_SIZE  18

static unsigned char g_area[AREA_SIZE];
static unsigned char g_source[AREA_SIZE];

typedef struct record
{
    char name[7];
    int numbers[5];
    short half;
} record_t;

static void fill(unsigned char *area, unsigned int seed)
{
    for (int i = 0; i < AREA_SIZE; ++i) {
        area[i] = (unsigned char) (seed + i * 7);
    }
}

/* One number that depends on every byte of the area and on its position. */
static unsigned int digest(const unsigned char *area)
{
    unsigned int result = 0;

    for (int i = 0; i < AREA_SIZE; ++i) {
        result = result * 31 + area[i];
    }

    return result;
}

static void test_memset(void)
{
    for (int offset = 0; offset < 4; ++offset) {
        unsigned int total = 0;

        for (int size = 0; size <= MAX_SIZE; ++size) {
            fill(g_area, (unsigned int) size);
            memset(g_area + 16 + offset, 0x3a5 + size, (unsigned int) size);
            total = total * 17 + digest(g_area);
        }
        show("memset", total);
    }
}

static void test_memcpy(void)
{
    fill(g_source, 100);

    for (int dst_offset = 0; dst_offset < 4; ++dst_offset) {
        for (int src_offset = 0; src_offset < 4; ++src_offset) {
            unsigned int total = 0;

            for (int size = 0; size <= MAX_SIZE; ++size) {
                fill(g_area, (unsigned int) size);
                memcpy(g_area + 16 + dst_offset, g_source + 8 + src_offset, (unsigned int) size);
                total = total * 17 + digest(g_area);
            }
            show("memcpy", total);
        }
    }
}

/* The source and the destination share bytes when the shift is shorter
   than the size. */
static void test_memmove(void)
{
    for (int shift = -9; shift <= 9; ++shift) {
        unsigned int total = 0;

        for (int size = 0; size <= MAX_SIZE; ++size) {
            fill(g_area, (unsigned int) size);
            memmove(g_area + 24 + shift, g_area + 24, (unsigned int) size);
            total = total * 17 + digest(g_area);
        }
        show("memmove", total);
    }
}

static void test_compiler(void)
{
    /* The parts without a starting value are zeros. */
    int partial[8] = { 1, 2 };
    record_t record = { "abc", { 5 }, -1 };
    record_t copy;

    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        sum = sum * 3 + partial[i];
    }
    show("partial", sum);
    show("record.name[2]", record.name[2]);
    show("record.name[6]", record.name[6]);
    show("record.numbers[0]", record.numbers[0]);
    show("record.numbers[4]", record.numbers[4]);
    show("record.half", record.half);

    copy = record;
    copy.numbers[4] = 9;
    record = copy;
    show("record.numbers[4]", record.numbers[4]);
    show("copy.name[0]", copy.name[0]);
    show("copy.half", copy.half);
}

int main(void)
{
    test_memset();
    test_memcpy();
    test_memmove();
    test_compiler();

    return 0;
}
