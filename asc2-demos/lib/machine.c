#include "machine.h"

#ifdef RACH2

#include "user.h"

void put_char(int c)
{
    char byte = (char) c;

    write(STDOUT, &byte, 1);
}

void put_str(const char *str)
{
    write(STDOUT, str, strlen(str));
}

/* The pictures that the kernel has named for the program, and the place of the next one; see `scr_blit` */
static unsigned int *named[SCR_PICTURES];
static int next_named;

int scr_open(void)
{
    int result = screen(SCREEN_OPEN, 0, 0);
    int i;

    if (result < 0) {
        return result;
    }
    for (i = 0; i < SCR_PICTURES; ++i) {
        named[i] = 0; /* the kernel forgets the pictures when the screen is given back */
    }

    return result == SCR_PAGE ? 0 : -1;
}

void scr_close(void)
{
    screen(SCREEN_CLOSE, 0, 0);
}

#else

void put_str(const char *str)
{
    while (*str != '\0') {
        put_char(*str);
        ++str;
    }
}

int scr_open(void)
{
    return 0;
}

void scr_close(void)
{
}

#endif

void put_dec(unsigned int value)
{
    char digits[10];
    int count = 0;

    do {
        digits[count++] = (char) ('0' + value % 10);
        value /= 10;
    } while (value != 0);

    while (count > 0) {
        put_char(digits[--count]);
    }
}

void scr_clear(void)
{
    RP_SCR_CMD = SCR_CLEAR;
}

void scr_show(void)
{
    RP_SCR_CMD = SCR_SHOW;
    while (RP_SCR_STAT != 0) {
    }
}

void scr_text(int row, int col, const char *str)
{
    unsigned int place = (unsigned int) row << 16 | (unsigned int) col << 8;
    while (*str != '\0') {
        RP_SCR_CHAR = place | (unsigned char) *str;
        ++str;
        place += 0x100;
        if ((place & 0xff00) == SCR_COLS << 8) {
            place = (place & 0xff0000) + 0x10000;
        }
    }
}

int scr_dec(int row, int col, unsigned int value)
{
    char digits[11];
    int count = 0;

    do {
        digits[count++] = (char) ('0' + value % 10);
        value /= 10;
    } while (value != 0);

    unsigned int place = (unsigned int) row << 16 | (unsigned int) col << 8;
    while (count > 0) {
        RP_SCR_CHAR = place | (unsigned char) digits[--count];
        place += 0x100;
        ++col;
    }

    return col;
}

void scr_rect(int x, int y, int width, int height, int mode)
{
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (width <= 0 || height <= 0 || x >= SCR_WIDTH || y >= SCR_HEIGHT) {
        return;
    }
    if (width > SCR_WIDTH) {
        width = SCR_WIDTH;
    }
    if (height > SCR_HEIGHT) {
        height = SCR_HEIGHT;
    }

    RP_SCR_RECT = (unsigned int) x | (unsigned int) y << 7 | (unsigned int) (width - 1) << 13 |
                  (unsigned int) (height - 1) << 21 | (unsigned int) mode << 27;
}

void scr_blit(unsigned int *picture, int x, int y, int mode)
{
    if (x < -128 || x > 127 || y < -128 || y > 127) {
        return;
    }

#ifdef RACH2
    /*
        The RP2040 draws out of RAM, and only the kernel knows where in RAM
        a picture is. The kernel keeps a few pictures named for the program;
        a picture that is not among them takes the place of the one that
        was named longest ago, which costs a call of the kernel.
    */
    int number = 0;

    while (number < SCR_PICTURES && named[number] != picture) {
        ++number;
    }
    if (number == SCR_PICTURES) {
        number = next_named;
        if (screen(SCREEN_PICTURE, (unsigned int) number, (unsigned int) picture) < 0) {
            return;
        }
        named[number] = picture;
        next_named = (next_named + 1) % SCR_PICTURES;
    }

    RP_SCR_BLIT = ((unsigned int) x & 0xff) | ((unsigned int) y & 0xff) << 8 | (unsigned int) mode << 16 |
                  (unsigned int) number << 18;
#else
    /* The RP2040 wants the physical address; the kernel half of the addresses is RAM moved up by 0x80000000 */
    RP_SCR_SRC = (unsigned int) picture - 0x80000000u;
    RP_SCR_BLIT = ((unsigned int) x & 0xff) | ((unsigned int) y & 0xff) << 8 | (unsigned int) mode << 16;
#endif
}
