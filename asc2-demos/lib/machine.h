/*
    The devices of the RP2040 as a C program sees them: the console, the
    clock, the push button, and the screen, all served by the bridge, as
    every device is; `../../asc2-emulated/asc.h` describes the registers.

    A program is built in one of two ways from the same source. As a program
    of the Rach2 kernel (with `RACH2` defined) it asks the kernel for the
    screen with `scr_open`; the kernel puts the page of the screen's devices
    into its memory, and the console of the machine has the screen again
    when the program ends. As a program of the bare machine it is alone and
    uses the registers of the kernel's side.
*/

#ifndef MACHINE_H
#define MACHINE_H

#define REG(addr) (*(volatile unsigned int *) (addr))

#ifdef RACH2

/* The device page that the kernel gives a program: the frame memory, then the registers */
#define SCR_PAGE      0x007ef000

#define RP_SCR_STAT   REG(SCR_PAGE + 0x400) /* 1 while a frame is on its way to the screen */
#define RP_SCR_CMD    REG(SCR_PAGE + 0x404) /* 1 = show the frame memory, 2 = clear it */
#define RP_SCR_FRAMES REG(SCR_PAGE + 0x408) /* frames that have reached the screen */
#define RP_SCR_CHAR   REG(SCR_PAGE + 0x40c) /* draws a character: row << 16 | column << 8 | character */
#define RP_SCR_RECT   REG(SCR_PAGE + 0x410) /* draws a rectangle; see `scr_rect` */
#define RP_SCR_BLIT   REG(SCR_PAGE + 0x414) /* draws a picture that the kernel knows; see `scr_blit` */
#define RP_BTN_STAT   REG(SCR_PAGE + 0x418) /* 1 while the button is down */
#define RP_BTN_COUNT  REG(SCR_PAGE + 0x41c) /* presses since the reset */
#define RP_TIME       REG(SCR_PAGE + 0x420) /* microseconds since the reset */
#define RP_SCR_MOVE   REG(SCR_PAGE + 0x424) /* keeps a point for the next line; see `SCR_POINT` */
#define RP_SCR_LINE   REG(SCR_PAGE + 0x428) /* draws a line from the point that was kept to this one, and keeps it */

#define SCR_MEM       ((volatile unsigned int *) SCR_PAGE) /* frame memory: 64 rows of 4 words, the leftmost pixel of a word in its bit 0 */

#else

#define RP_TIME       REG(0x80ff000c) /* microseconds since the reset */
#define RP_BTN_STAT   REG(0x80ff0034) /* 1 while the button is down */
#define RP_BTN_COUNT  REG(0x80ff0038) /* presses since the reset */
#define RP_SCR_STAT   REG(0x80ff003c) /* 1 while a frame is on its way to the screen */
#define RP_SCR_CMD    REG(0x80ff0040) /* 1 = show the frame memory, 2 = clear it */
#define RP_SCR_FRAMES REG(0x80ff0044) /* frames that have reached the screen */
#define RP_SCR_CHAR   REG(0x80ff0048) /* draws a character: row << 16 | column << 8 | character */
#define RP_SCR_RECT   REG(0x80ff004c) /* draws a rectangle; see `scr_rect` */
#define RP_SCR_SRC    REG(0x80ff0050) /* physical address of the picture for `RP_SCR_BLIT` */
#define RP_SCR_BLIT   REG(0x80ff0054) /* draws that picture; see `scr_blit` */
#define RP_SCR_MOVE   REG(0x80ff0078) /* keeps a point for the next line; see `SCR_POINT` */
#define RP_SCR_LINE   REG(0x80ff007c) /* draws a line from the point that was kept to this one, and keeps it */

#define SCR_MEM       ((volatile unsigned int *) 0x80ff1000) /* frame memory: 64 rows of 4 words, the leftmost pixel of a word in its bit 0 */

#endif

#define SCR_PICTURES  4  /* pictures that a program of the kernel can use in turn without a call of the kernel */
#define SCR_WIDTH     128
#define SCR_HEIGHT    64
#define SCR_COLS      21 /* characters in a row of text */
#define SCR_ROWS      8  /* rows of text */

#define SCR_SHOW      1
#define SCR_CLEAR     2

/* How a rectangle or a picture changes the pixels under it */
#define SCR_DARK      0 /* off */
#define SCR_LIGHT     1 /* on */
#define SCR_FLIP      2 /* each pixel becomes its opposite */
#define SCR_COPY      3 /* a picture only: its pixels replace those of the screen */

/* The value of a point for `RP_SCR_MOVE` and `RP_SCR_LINE`. It may lie outside the screen, from -512 to 511; the parts of a line outside the screen are left out. */
#define SCR_POINT(x, y) (((unsigned int) (x) & 0xffff) | (unsigned int) (y) << 16)

/* The value of one lit pixel for `RP_SCR_RECT`: a rectangle of 1 by 1. The next column is SCR_PIXEL_X more, the next row SCR_PIXEL_Y. */
#define SCR_PIXEL(x, y) ((unsigned int) (x) | (unsigned int) (y) << 7 | (unsigned int) SCR_LIGHT << 27)
#define SCR_PIXEL_X     1
#define SCR_PIXEL_Y     128

/* Console */

void put_char(int c);
void put_str(const char *str);
void put_dec(unsigned int value);

/* Screen */

/*
    Takes the screen, the button, and the clock for the program. Nothing of
    the screen may be used before. Returns 0, or a negative number if the
    kernel refuses: another program has the screen. On the bare machine the
    screen is the program's from the start.
*/
int scr_open(void);

/* Gives the screen back to the console of the machine. */
void scr_close(void);

/* Makes the frame memory dark. */
void scr_clear(void);

/* Sends the frame memory to the screen and returns when it is there. */
void scr_show(void);

/* Draws a text from a column of a row of text on; it goes on in the next row. */
void scr_text(int row, int col, const char *str);

/* Draws a number at a column of a row of text and returns the column after it. */
int scr_dec(int row, int col, unsigned int value);

/*
    Draws a rectangle; `mode` is `SCR_DARK`, `SCR_LIGHT`, or `SCR_FLIP`. The
    parts outside the screen are left out, also at the left and at the top.
*/
void scr_rect(int x, int y, int width, int height, int mode);

/*
    Draws a picture with its left top corner at a column and a row, which
    may lie outside the screen. A picture is an array of words in RAM (so
    not `const`): the width in bits 15:0 and the height in bits 31:16 of the
    first word, then the rows from the top, each in as many words as the
    width needs, the leftmost pixel in bit 0. With `SCR_DARK`, `SCR_LIGHT`,
    and `SCR_FLIP` only the lit pixels of the picture count; with `SCR_COPY`
    the pixels of the screen become those of the picture.
*/
void scr_blit(unsigned int *picture, int x, int y, int mode);

#endif
