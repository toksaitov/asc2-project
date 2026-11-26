/*
    How fast is the machine with its screen? This program measures it with
    the clock of the RP2040, prints the numbers on the console, and shows
    the most telling ones on the screen.

    Every instruction and every access of the chip is a talk with the RP2040
    over the bus, so the numbers are those of the bus more than of the CPU.
*/

#include "lib/machine.h"
#include "pictures.h"

static unsigned int start;

/* The small picture of the measurement, a stone of 12 by 8 pixels; the pictures of `pictures.h` are larger */
static unsigned int stone[] = {
    12 | 8 << 16,
    0x000003fc,
    0x000007fe,
    0x00000fff,
    0x00000fff,
    0x00000fff,
    0x00000fff,
    0x000007fe,
    0x000003fc
};

/* Prints how many times a thing was done since `start`, and returns how many times that is in a second */
static unsigned int report(const char *what, unsigned int count)
{
    unsigned int us = RP_TIME - start;
    /* count * 1000000 does not fit into 32 bits; the time in units of 100 microseconds does */
    unsigned int rate = count * 10000 / (us / 100 + 1);

    put_str(what);
    put_str(": ");
    put_dec(count);
    put_str(" in ");
    put_dec(us / 1000);
    put_str(" ms, ");
    put_dec(rate);
    put_str(" in a second\n");

    return rate;
}

int main(void)
{
    unsigned int i;
    unsigned int text_rate;
    unsigned int pixels_rate;
    unsigned int film_rate;
    unsigned int show_rate;
    unsigned int place;
    int x;

    put_str("speed of Almurut II with its screen\n");

    /* Work of the CPU alone: a loop that touches no memory */
    start = RP_TIME;
    for (i = 0; i < 20000; ++i) {
    }
    report("turns of an empty loop", 20000);

    /* What one write to a register of the screen costs, and what it draws */
    start = RP_TIME;
    for (i = 0; i < 4096; ++i) {
        SCR_MEM[i & 255] = i;
    }
    report("words of 32 pixels", 4096);

    scr_clear();
    start = RP_TIME;
    place = 0;
    for (i = 0; i < 2000; ++i) {
        RP_SCR_CHAR = place | ('A' + (i & 15));
        place = (place + 0x100) & 0x70f00;
    }
    report("characters", 2000);

    start = RP_TIME;
    for (i = 0; i < 2000; ++i) {
        scr_rect((int) (i & 63), (int) (i & 31), 16, 16, SCR_FLIP);
    }
    report("rectangles of 16 by 16 pixels", 2000);

    start = RP_TIME;
    for (i = 0; i < 2000; ++i) {
        scr_blit(stone, (int) (i & 127) - 6, (int) (i & 63) - 4, SCR_FLIP);
    }
    report("pictures of 12 by 8 pixels", 2000);

    start = RP_TIME;
    for (i = 0; i < 1000; ++i) {
        scr_blit(mountains, 0, (int) (i & 31), SCR_COPY);
    }
    report("pictures of 128 by 24 pixels", 1000);

    /* The way of a frame to the screen and nothing else */
    start = RP_TIME;
    for (i = 0; i < 200; ++i) {
        SCR_MEM[0] = i;
        scr_show();
    }
    show_rate = report("frames with one word changed", 200);

    /* Frames of text: 8 rows of 21 characters, all new each time */
    start = RP_TIME;
    for (i = 0; i < 30; ++i) {
        unsigned int code = '!' + (i & 31);
        for (place = 0; place < 0x80000; place += 0x10000) {
            unsigned int col;
            for (col = 0; col < SCR_COLS << 8; col += 0x100) {
                RP_SCR_CHAR = place | col | code;
                ++code;
            }
        }
        scr_show();
    }
    text_rate = report("frames of text, all 168 characters new", 30);

    /* Frames of pixels that the program sets itself: all 256 words new each time */
    start = RP_TIME;
    for (i = 0; i < 30; ++i) {
        unsigned int word;
        unsigned int pattern = 0x0f0f0f0fu << (i & 7);
        for (word = 0; word < 256; ++word) {
            SCR_MEM[word] = pattern;
        }
        scr_show();
    }
    pixels_rate = report("frames of pixels, all 256 words new", 30);

    /* A film that the RP2040 draws: the land, the ground, and a burkut that flies across */
    start = RP_TIME;
    i = 0;
    for (x = -12; x < SCR_WIDTH; ++x) {
        scr_clear();
        scr_blit(mountains, 0, 38, SCR_LIGHT);
        scr_rect(0, 62, SCR_WIDTH, 2, SCR_LIGHT);
        scr_blit((x & 4) ? burkut_down : burkut_up, x, 20, SCR_LIGHT);
        scr_show();
        ++i;
    }
    film_rate = report("frames of a film of pictures", i);

    scr_clear();
    scr_text(0, 0, "frames in a second");
    scr_text(2, 0, "one word");
    scr_dec(2, 14, show_rate);
    scr_text(3, 0, "film");
    scr_dec(3, 14, film_rate);
    scr_text(4, 0, "full text");
    scr_dec(4, 14, text_rate);
    scr_text(5, 0, "full pixels");
    scr_dec(5, 14, pixels_rate);
    scr_show();

    put_str("done\n");

    return 0;
}
