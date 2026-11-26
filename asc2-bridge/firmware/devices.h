/*
    The memory and the devices that the bridge serves to the chip: RAM, the
    boot ROM, and the registers of the console, the disk, the clock, the
    push button, and the screen.

    `asc2-emulated/asc.h` defines them: the `rp_t` structure and the functions
    `bus_access`, `rp_access`, and `rp_access_reg`. This file does the same
    for two cores of a microcontroller, and `test/` compares the two on random
    requests. The names are the emulator's, except for the boot ROM: the SDK
    of the microcontroller has a `ROM_BASE` of its own.

    Core 1 answers the bus. It runs from RAM and may not call into the flash,
    so its functions here are inline and have no `switch`, no division, and no
    library call. Core 0 feeds the console and does the work of the disk. A
    field that both cores use has one core that writes it.
*/
#ifndef DEVICES_H
#define DEVICES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The firmware forces its inline functions into the code of core 1 and orders
// the writes of the two cores; a test on another computer needs neither
#ifndef BRIDGE_INLINE
#define BRIDGE_INLINE static inline
#endif
#ifndef BRIDGE_BARRIER
#define BRIDGE_BARRIER()
#endif
// The firmware keeps the tables of core 1 in RAM
#ifndef BRIDGE_TABLE
#define BRIDGE_TABLE static const
#endif

/* Physical Memory */

#define RAM_SIZE         0x30000   // amount of RAM (192 KB)
#define BOOT_ROM_BASE    0x800000  // physical address of the boot ROM
#define BOOT_ROM_SIZE    0x40000   // size of the boot ROM's range (256 KB); the bytes after an image are zeros
#define RP_BASE          0xFF0000  // physical address of the device registers
#define RP_SIZE          0x10000   // size of the range of the device registers
#define HIGH_BASE        0xFFFFF000 // physical address of the high page: the last page of the 32-bit physical address space, which starts with a little RAM
#define HIGH_SIZE        0x400     // amount of RAM at the start of the high page (1 KB)

/* Device Registers */

#define RP_ID_ADDR         0xFF0000 // read-only register with the bytes "ALM2"
#define RP_SCRATCH_ADDR    0xFF0004 // read-write register with no effect, to test the bus
#define RP_RAM_SIZE_ADDR   0xFF0008 // read-only register with the amount of RAM in bytes
#define RP_TIME_ADDR       0xFF000C // read-only register with the number of microseconds since reset
#define RP_CON_STAT_ADDR   0xFF0010 // console's status register
#define RP_CON_DATA_ADDR   0xFF0014 // console's data register: a read takes the next input byte (0x100 if there is none), a write prints a byte
#define RP_IRQ_STAT_ADDR   0xFF0018 // devices that request an interrupt now; writing 1 to the disk's bit clears it
#define RP_IRQ_ENABLE_ADDR 0xFF001C // devices that are allowed to raise the interrupt line
#define RP_DISK_SEC_ADDR   0xFF0020 // disk's sector number for the next command
#define RP_DISK_BUF_ADDR   0xFF0024 // physical address in RAM of a buffer of one sector for the next command
#define RP_DISK_CMD_ADDR   0xFF0028 // disk's command register: writing 1 reads the sector into the buffer, writing 2 writes the buffer to the sector
#define RP_DISK_STAT_ADDR  0xFF002C // disk's status register
#define RP_LOSE_ADDR       0xFF0030 // test register: a write of n makes the bridge do the n-th access after this one and never answer it; reads as the number of accesses left
#define RP_BTN_STAT_ADDR   0xFF0034 // read-only register of the push button: bit 0 is set while the button is down
#define RP_BTN_COUNT_ADDR  0xFF0038 // read-only register with the number of presses of the button since reset
#define RP_SCR_STAT_ADDR   0xFF003C // screen's status register
#define RP_SCR_CMD_ADDR    0xFF0040 // screen's command register: writing 1 shows the frame memory on the screen, writing 2 clears the frame memory
#define RP_SCR_FRAMES_ADDR 0xFF0044 // read-only register with the number of frames that have reached the screen since reset
#define RP_SCR_CHAR_ADDR   0xFF0048 // write-only register that draws one character into the frame memory: the character in bits 7:0, its column (0 to 20) in bits 15:8, and its row (0 to 7) in bits 23:16
#define RP_SCR_RECT_ADDR   0xFF004C // write-only register that draws a rectangle into the frame memory: its left column in bits 6:0, its top row in bits 12:7, its width less 1 in bits 20:13, its height less 1 in bits 26:21, and the way to draw in bits 28:27
#define RP_SCR_SRC_ADDR    0xFF0050 // physical address in RAM of the picture that the next write to `RP_SCR_BLIT_ADDR` draws
#define RP_SCR_BLIT_ADDR   0xFF0054 // write-only register that draws the picture into the frame memory: the column of its left edge in bits 7:0 and the row of its top edge in bits 15:8, both signed, and the way to draw in bits 17:16
#define RP_SCR_PIC_ADDR    0xFF0058 // the first of eight registers that name four pictures for user programs: two registers for a picture, the physical address in RAM of the picture and the physical address of the page where it goes on if it does not end in its first page (0 if the pages lie one after the other)
#define RP_SCR_MOVE_ADDR   0xFF0078 // write-only register that keeps a point for the next line: x in bits 15:0 and y in bits 31:16, both signed
#define RP_SCR_LINE_ADDR   0xFF007C // write-only register that draws a lit line from the point that was kept to this one, and keeps it
#define RP_SCR_PICS        4        // number of these pictures
#define RP_USER_BASE       0xFF1000 // physical address of the page of the screen and the button, the only device page that takes accesses in user mode
#define RP_USER_SIZE       0x1000   // size of that page
#define RP_SCR_BASE        0xFF1000 // physical address of the frame memory of the screen
#define RP_USER_SCR_STAT_ADDR   0xFF1400 // registers in the user page that work as those above: the screen's status,
#define RP_USER_SCR_CMD_ADDR    0xFF1404 // its commands "show" and "clear",
#define RP_USER_SCR_FRAMES_ADDR 0xFF1408 // the number of frames,
#define RP_USER_SCR_CHAR_ADDR   0xFF140C // a character,
#define RP_USER_SCR_RECT_ADDR   0xFF1410 // a rectangle,
#define RP_USER_SCR_BLIT_ADDR   0xFF1414 // a picture, one of the four that the kernel has named, with its number in bits 19:18,
#define RP_USER_BTN_STAT_ADDR   0xFF1418 // the level of the button,
#define RP_USER_BTN_COUNT_ADDR  0xFF141C // the number of its presses,
#define RP_USER_TIME_ADDR       0xFF1420 // the clock in microseconds,
#define RP_USER_SCR_MOVE_ADDR   0xFF1424 // and the two registers of the lines: the point,
#define RP_USER_SCR_LINE_ADDR   0xFF1428 // and the line to it
#define RP_PAGE_SIZE       0x1000   // size of a page of the machine's virtual memory
#define RP_SCR_SIZE        0x400    // size of the frame memory: 64 rows of 16 bytes, a bit per pixel
#define RP_SCR_WIDTH       128      // pixels in a row of the screen
#define RP_SCR_HEIGHT      64       // rows of the screen
#define RP_SCR_CHAR_WIDTH  6        // pixels that a character takes in a row: five of its own and an empty one
#define RP_SCR_CHAR_HEIGHT 8        // rows that a character takes: seven of its own and an empty one
#define RP_SCR_POINT_MIN   (-512)   // the points of a line lie from here
#define RP_SCR_POINT_MAX   511      // to here
#define RP_SCR_COLS        21       // characters in a row of text
#define RP_SCR_ROWS        8        // rows of text

#define RP_ID              0x324d4c41 /* "ALM2" */
#define RP_CON_FLAG_INPUT  0b00000001
#define RP_CON_FLAG_OUTPUT 0b00000010
#define RP_CON_NO_DATA     0x100
#define RP_IRQ_FLAG_CON    0b00000001
#define RP_IRQ_FLAG_DISK   0b00000010
#define RP_IRQ_FLAG_BTN    0b00000100
#define RP_IRQ_FLAGS       (RP_IRQ_FLAG_CON | RP_IRQ_FLAG_DISK | RP_IRQ_FLAG_BTN)
#define RP_BTN_FLAG_DOWN   0b00000001
#define RP_SCR_FLAG_BUSY   0b00000001
#define RP_SCR_CMD_SHOW    1
#define RP_SCR_CMD_CLEAR   2
#define RP_SCR_CMD_CONSOLE 3
#define RP_SCR_DARK        0    // ways to draw: the pixels go dark,
#define RP_SCR_LIGHT       1    // they light up,
#define RP_SCR_FLIP        2    // each becomes its opposite,
#define RP_SCR_COPY        3    // or (a picture only) they become the pixels of the picture, lit and dark
#define RP_FONT_FIRST      0x20 // first and last character of the font; any other is drawn as a question mark
#define RP_FONT_LAST       0x7e
#define RP_DISK_CMD_READ   1
#define RP_DISK_CMD_WRITE  2
#define RP_DISK_FLAG_BUSY  0b00000001
#define RP_DISK_FLAG_ERROR 0b00000010
#define RP_DISK_SEC_SIZE   0x1000 // size of a disk sector in bytes

#define CON_IN_SIZE        256    // number of console input bytes that the bridge holds for the CPU
#define CON_OUT_SIZE       512    // number of console output bytes that the bridge holds for the terminal

// The characters of the screen: five columns each, the top pixel of a column in its bit 0
BRIDGE_TABLE uint8_t BRIDGE_FONT[RP_FONT_LAST - RP_FONT_FIRST + 1][RP_SCR_CHAR_WIDTH - 1] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5f, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7f, 0x14, 0x7f, 0x14}, {0x24, 0x2a, 0x7f, 0x2a, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62}, {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1c, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1c, 0x00}, {0x14, 0x08, 0x3e, 0x08, 0x14}, {0x08, 0x08, 0x3e, 0x08, 0x08}, {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00}, {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14}, {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a},
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01}, {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7f, 0x41, 0x41, 0x00}, {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7f, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78}, {0x7f, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7f}, {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7e, 0x09, 0x01, 0x02}, {0x0c, 0x52, 0x52, 0x52, 0x3e},
    {0x7f, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7d, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3d, 0x00}, {0x7f, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7f, 0x40, 0x00}, {0x7c, 0x04, 0x18, 0x04, 0x78}, {0x7c, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7c, 0x14, 0x14, 0x14, 0x08}, {0x08, 0x14, 0x14, 0x18, 0x7c}, {0x7c, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20}, {0x04, 0x3f, 0x44, 0x40, 0x20}, {0x3c, 0x40, 0x40, 0x20, 0x7c}, {0x1c, 0x20, 0x40, 0x20, 0x1c}, {0x3c, 0x40, 0x30, 0x40, 0x3c},
    {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0c, 0x50, 0x50, 0x50, 0x3c}, {0x44, 0x64, 0x54, 0x4c, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7f, 0x00, 0x00}, {0x00, 0x41, 0x36, 0x08, 0x00}, {0x08, 0x04, 0x08, 0x10, 0x08},
};

/* Header of a Request and Answers of the Bus */

#define HDR_WRITE          0b10000000 // the request is a write
#define HDR_NEXT           0b01000000 // a fetch of "the next word", which carries no address
#define HDR_SIZE_SHIFT     4          // two bits: the number of bytes is 1 shifted left by them
#define HDR_SIZE_MASK      0b11
#define HDR_FETCH          0b00001000 // the read is an instruction fetch
#define HDR_KERNEL         0b00000100 // the CPU is in kernel mode
#define HDR_SEQ            0b00000001 // sequence bit, which changes with every new request

#define BUS_STAT_OK         0 // the access was done
#define BUS_STAT_ERROR      1 // nothing answers at the address, or the device there refuses this kind of access
#define BUS_STAT_PROTECTION 2 // the address is for kernel mode only, or it holds no instructions and the access is a fetch
#define BUS_STAT_MISALIGNED 3 // the address is not a multiple of the access size
#define BUS_STAT_BAD_NEXT   4 // "the next word" was asked for with no earlier fetch to continue

/* Device */

typedef struct bridge
{
    uint8_t *ram;     // `RAM_SIZE` bytes
    uint8_t *rom;     // image of the boot ROM
    uint8_t *high;    // `HIGH_SIZE` bytes of the RAM in the high page
    uint32_t rom_len; // number of bytes in `rom`, a multiple of 4; the rest of the ROM's range reads as zeros

    /* Written by core 1 */

    uint32_t scratch;    // value of the scratch register
    uint32_t irq_enable; // devices that are allowed to raise the interrupt line

    uint32_t disk_sec; // sector number for the next command
    uint32_t disk_buf; // physical address of the buffer for the next command
    uint32_t disk_cmd; // last command written

    volatile uint32_t disk_started; // number of commands started; the disk is busy while it differs from `disk_done`
    uint32_t disk_irq_seen;         // value of `disk_done` when the CPU last cleared the disk's interrupt request

    uint32_t fetch_addr; // physical address after the last instruction fetch, where a fetch of "the next word" reads
    bool fetch_valid;    // there was such a fetch, and it has succeeded
    uint32_t lose_cnt;   // number of accesses until the one whose answer gets lost (0 if none is planned)

    uint32_t btn_irq_seen; // value of `btn_count` when the CPU last cleared the button's interrupt request
    uint32_t scr_point;    // the point that the next line starts at, as `RP_SCR_MOVE_ADDR` takes it
    uint32_t scr_src;      // physical address of the picture for the next write to `RP_SCR_BLIT_ADDR`
    uint32_t scr_pic[RP_SCR_PICS][2]; // the pictures for user programs: the address of the start, and of the page where it goes on

    /*
        The screen is the console's until a program draws: every byte that
        the CPU prints is drawn into the frame memory as a terminal does,
        and core 0 shows the frame memory by itself when `scr_drawn` has
        changed. The emulator's `asc.h` tells the rules.
    */
    volatile uint32_t scr_console; // the screen is the console's
    volatile uint32_t scr_drawn;   // number of times that the console has drawn
    uint32_t con_row;              // place of the console's cursor
    uint32_t con_col;
    uint32_t con_esc;              // the console is inside an escape sequence: 1 after the escape byte, 2 after the bracket

    uint32_t scr[RP_SCR_SIZE / 4];      // frame memory, in words with the low byte first
    uint32_t scr_sent[RP_SCR_SIZE / 4]; // the copy of the frame memory that the last command "show" has taken; core 0 reads it while the screen is busy
    volatile uint32_t scr_started;      // number of commands "show" taken; the screen is busy while it differs from `scr_done`

    volatile uint32_t con_in_taken; // number of input bytes that the CPU has taken
    volatile uint32_t con_out_put;  // number of output bytes that the CPU has printed
    volatile uint32_t con_out_lost; // number of output bytes dropped because nobody took the earlier ones

    /* Written by core 0 */

    volatile uint32_t disk_done;     // number of commands finished
    volatile uint32_t disk_error;    // the last finished command named a sector outside the disk or a buffer outside RAM
    volatile uint32_t con_in_put;    // number of input bytes typed
    volatile uint32_t con_out_taken; // number of output bytes sent to the terminal
    volatile uint32_t btn_down;      // the button is down
    volatile uint32_t btn_count;     // number of presses
    volatile uint32_t scr_done;      // number of frames that have reached the screen

    uint8_t con_in[CON_IN_SIZE];   // input bytes, at the index of their number modulo the size
    uint8_t con_out[CON_OUT_SIZE]; // output bytes, the same way
} bridge_t;

/* Initialization Functions */

// Puts the devices into their state after a reset of the chip. RAM and the
// ROM image are kept. Core 1 must not run meanwhile.
static inline void bridge_reset(bridge_t *b)
{
    b->scratch = 0;
    b->irq_enable = 0; // no device raises the interrupt line at boot

    b->disk_sec = 0;
    b->disk_buf = 0;
    b->disk_cmd = 0;
    b->disk_started = 0;
    b->disk_irq_seen = 0;
    b->disk_done = 0;
    b->disk_error = 0;

    b->fetch_addr = 0;
    b->fetch_valid = false;
    b->lose_cnt = 0;

    b->btn_irq_seen = 0;
    b->scr_point = 0;
    b->scr_src = 0;
    for (uint32_t i = 0; i < RP_SCR_PICS; ++i) {
        b->scr_pic[i][0] = 0;
        b->scr_pic[i][1] = 0;
    }
    b->scr_console = 1; // the console draws until a program does
    b->scr_drawn = b->scr_drawn + 1; // core 0 shows the empty console
    b->con_row = 0;
    b->con_col = 0;
    b->con_esc = 0;
    b->btn_down = 0;
    b->btn_count = 0;

    for (uint32_t i = 0; i < RP_SCR_SIZE / 4; ++i) { // the frame memory is dark at boot
        b->scr[i] = 0;
        b->scr_sent[i] = 0;
    }
    b->scr_started = 0;
    b->scr_done = 0;

    b->con_in_put = 0;
    b->con_in_taken = 0;
    b->con_out_put = 0;
    b->con_out_taken = 0;
    b->con_out_lost = 0;
}

/* Console */

// Core 0: gives the CPU a typed byte. Returns false if the queue is full; the
// sender has to wait and try again.
static inline bool bridge_put_con_input(bridge_t *b, uint8_t byte)
{
    uint32_t put = b->con_in_put;
    if (put - b->con_in_taken == CON_IN_SIZE) {
        return false;
    }

    b->con_in[put % CON_IN_SIZE] = byte;
    BRIDGE_BARRIER();
    b->con_in_put = put + 1;

    return true;
}

// Core 0: takes a byte that the CPU has printed. Returns false if there is none.
static inline bool bridge_take_con_output(bridge_t *b, uint8_t *byte)
{
    uint32_t taken = b->con_out_taken;
    if (taken == b->con_out_put) {
        return false;
    }

    *byte = b->con_out[taken % CON_OUT_SIZE];
    BRIDGE_BARRIER();
    b->con_out_taken = taken + 1;

    return true;
}

/* Disk */

// Core 0: tells whether a command is waiting for its work.
static inline bool bridge_disk_is_busy(bridge_t *b)
{
    return b->disk_started != b->disk_done;
}

// Core 0: tells whether the waiting command names a sector of the disk and a
// buffer in RAM. A running command keeps its registers.
static inline bool bridge_disk_is_valid(bridge_t *b, uint32_t disk_secs)
{
    return (b->disk_cmd == RP_DISK_CMD_READ || b->disk_cmd == RP_DISK_CMD_WRITE) &&
           b->disk_sec < disk_secs &&
           b->disk_buf % 4 == 0 &&
           b->disk_buf <= RAM_SIZE - RP_DISK_SEC_SIZE;
}

// Core 0: ends the waiting command, which requests an interrupt.
static inline void bridge_disk_finish(bridge_t *b, bool is_valid)
{
    b->disk_error = is_valid ? 0 : 1;
    BRIDGE_BARRIER();
    b->disk_done = b->disk_done + 1;
}

/* Button and Screen */

// Changes the pixels of a word of the frame memory that `bits` names in one
// of the ways to draw. `cover` names the pixels that the way "copy" sets.
BRIDGE_INLINE void bridge_draw_word(uint32_t *word, uint32_t bits, uint32_t cover, uint32_t mode)
{
    if (mode == RP_SCR_DARK) {
        *word &= ~bits;
    } else if (mode == RP_SCR_LIGHT) {
        *word |= bits;
    } else if (mode == RP_SCR_FLIP) {
        *word ^= bits;
    } else {
        *word = (*word & ~cover) | bits;
    }
}

/*
    Draws up to 32 pixels of a row of the frame memory: `bits` are the pixels
    and `cover` the places that belong to the picture, bit 0 at the column
    `x`, which may lie left of the screen. Core 1 has no 64-bit shifts, so
    the two words that the pixels may reach are worked out one by one.
*/
BRIDGE_INLINE void bridge_draw_bits(uint32_t *row, int32_t x, uint32_t bits, uint32_t cover, uint32_t mode)
{
    if (x <= -32 || x >= RP_SCR_WIDTH) {
        return;
    }
    if (x < 0) {
        bits >>= (uint32_t) -x;
        cover >>= (uint32_t) -x;
        x = 0;
    }

    uint32_t index = (uint32_t) x >> 5;
    uint32_t shift = (uint32_t) x & 31;
    bridge_draw_word(row + index, bits << shift, cover << shift, mode);
    if (shift != 0 && index + 1 < RP_SCR_WIDTH / 32) {
        bridge_draw_word(row + index + 1, bits >> (32 - shift), cover >> (32 - shift), mode);
    }
}

// Draws a rectangle into the frame memory. The way "copy" has no meaning
// for a rectangle and draws nothing.
BRIDGE_INLINE void bridge_draw_rect(uint32_t *scr, uint32_t value)
{
    uint32_t left   = value & 0x7f;
    uint32_t top    = (value >> 7) & 0x3f;
    uint32_t width  = ((value >> 13) & 0xff) + 1;
    uint32_t height = ((value >> 21) & 0x3f) + 1;
    uint32_t mode   = (value >> 27) & 0x3;
    if (mode == RP_SCR_COPY) {
        return;
    }
    if (width > RP_SCR_WIDTH - left) {
        width = RP_SCR_WIDTH - left;
    }
    if (height > RP_SCR_HEIGHT - top) {
        height = RP_SCR_HEIGHT - top;
    }

    for (uint32_t y = top; y < top + height; ++y) {
        uint32_t *row = scr + y * (RP_SCR_WIDTH / 32);
        uint32_t x = left;
        uint32_t rest = width;
        while (rest != 0) {
            uint32_t cnt = rest < 32 ? rest : 32;
            uint32_t bits = cnt == 32 ? 0xffffffffu : (1u << cnt) - 1;
            bridge_draw_bits(row, (int32_t) x, bits, bits, mode);
            x += cnt;
            rest -= cnt;
        }
    }
}

/*
    Draws a lit line into the frame memory, from the point `from` to the point
    `to`, with both ends; see `rp_draw_line` of the emulator. A line with an
    end outside of the points of a line draws nothing.
*/
BRIDGE_INLINE void bridge_draw_line(uint32_t *scr, uint32_t from, uint32_t to)
{
    int32_t x0 = (int16_t) (from & 0xffff);
    int32_t y0 = (int16_t) (from >> 16);
    int32_t x1 = (int16_t) (to & 0xffff);
    int32_t y1 = (int16_t) (to >> 16);
    if (x0 < RP_SCR_POINT_MIN || x0 > RP_SCR_POINT_MAX || y0 < RP_SCR_POINT_MIN || y0 > RP_SCR_POINT_MAX ||
        x1 < RP_SCR_POINT_MIN || x1 > RP_SCR_POINT_MAX || y1 < RP_SCR_POINT_MIN || y1 > RP_SCR_POINT_MAX) {
        return;
    }

    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int32_t step_x = x1 > x0 ? 1 : -1;
    int32_t step_y = y1 > y0 ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < RP_SCR_WIDTH && y0 >= 0 && y0 < RP_SCR_HEIGHT) {
            scr[y0 * (RP_SCR_WIDTH / 32) + (x0 >> 5)] |= 1u << (x0 & 31);
        }
        if (x0 == x1 && y0 == y1) {
            return;
        }
        int32_t twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += step_x;
        }
        if (twice <= dx) {
            error += dx;
            y0 += step_y;
        }
    }
}

// Returns the word `index` of a picture that starts at the physical address
// `src` and goes on, after the end of its first page, at `next` (0 if the
// pages lie one after the other).
BRIDGE_INLINE uint32_t bridge_get_picture_word(const uint8_t *ram, uint32_t src, uint32_t next, uint32_t index)
{
    uint32_t in_page = (src & (RP_PAGE_SIZE - 1)) + index * 4;
    uint32_t addr = (next != 0 && in_page >= RP_PAGE_SIZE) ? next + (in_page - RP_PAGE_SIZE) : src + index * 4;

    return *(const uint32_t *) (const void *) (ram + addr); // RAM starts at a multiple of 4
}

// Draws the picture at the physical address `src` into the frame memory;
// `next` is the address of the page where the picture goes on, or 0. A
// picture that is not whole in RAM, whose address is not a multiple of 4,
// or that is empty or larger than the screen draws nothing.
BRIDGE_INLINE void bridge_draw_picture(uint32_t *scr, const uint8_t *ram, uint32_t src, uint32_t next, uint32_t value)
{
    int32_t left  = (int8_t) (value & 0xff);
    int32_t top   = (int8_t) ((value >> 8) & 0xff);
    uint32_t mode = (value >> 16) & 0x3;

    if ((src & 3) != 0 || src > RAM_SIZE - 4) {
        return;
    }
    if (next != 0 && ((next & (RP_PAGE_SIZE - 1)) != 0 || next > RAM_SIZE - RP_PAGE_SIZE)) {
        return;
    }
    uint32_t first = bridge_get_picture_word(ram, src, next, 0);
    uint32_t width  = first & 0xffff;
    uint32_t height = first >> 16;
    uint32_t row_words = (width + 31) >> 5;
    if (width == 0 || width > RP_SCR_WIDTH || height == 0 || height > RP_SCR_HEIGHT ||
        (next == 0 && row_words * 4 * height > RAM_SIZE - 4 - src)) {
        return;
    }

    uint32_t index = 1;
    for (uint32_t j = 0; j < height; ++j, index += row_words) {
        int32_t y = top + (int32_t) j;
        if (y < 0 || y >= RP_SCR_HEIGHT) {
            continue;
        }
        uint32_t *row = scr + (uint32_t) y * (RP_SCR_WIDTH / 32);
        uint32_t rest = width;
        for (uint32_t k = 0; k < row_words; ++k) {
            uint32_t cnt = rest < 32 ? rest : 32;
            uint32_t cover = cnt == 32 ? 0xffffffffu : (1u << cnt) - 1;
            bridge_draw_bits(row, left + (int32_t) (k * 32), bridge_get_picture_word(ram, src, next, index + k) & cover, cover, mode);
            rest -= cnt;
        }
    }
}

// Core 0: gives the level of the button. A press is counted and requests an interrupt.
static inline void bridge_set_button(bridge_t *b, bool is_down)
{
    if (is_down && !b->btn_down) {
        b->btn_count = b->btn_count + 1;
    }
    BRIDGE_BARRIER();
    b->btn_down = is_down ? 1 : 0;
}

// Core 0: tells whether a frame waits for its way to the screen; it is in `scr_sent`.
static inline bool bridge_screen_is_busy(bridge_t *b)
{
    return b->scr_started != b->scr_done;
}

// Core 0: the frame has reached the screen.
static inline void bridge_screen_finish(bridge_t *b)
{
    b->scr_done = b->scr_done + 1;
}

/* The Bridge's Side of the Bus (Core 1) */

// Draws a character into the frame memory at a column and a row of text.
// A place outside the screen draws nothing.
BRIDGE_INLINE void bridge_draw_char(uint32_t *scr, uint32_t value)
{
    uint32_t code = value & 0xff;
    uint32_t col  = (value >> 8) & 0xff;
    uint32_t row  = (value >> 16) & 0xff;
    if (col >= RP_SCR_COLS || row >= RP_SCR_ROWS) {
        return;
    }
    if (code < RP_FONT_FIRST || code > RP_FONT_LAST) {
        code = '?';
    }

    // A character lies in one word of a row or in two neighbours
    uint32_t x = col * RP_SCR_CHAR_WIDTH;
    uint32_t shift = x & 31;
    uint32_t *words = scr + row * RP_SCR_CHAR_HEIGHT * (RP_SCR_WIDTH / 32) + (x >> 5);
    uint32_t mask_low = (uint32_t) ((1u << RP_SCR_CHAR_WIDTH) - 1) << shift;
    uint32_t mask_high = shift > 32 - RP_SCR_CHAR_WIDTH ? ((1u << RP_SCR_CHAR_WIDTH) - 1) >> (32 - shift) : 0;
    for (uint32_t j = 0; j < RP_SCR_CHAR_HEIGHT; ++j) {
        uint32_t bits = 0;
        for (uint32_t i = 0; i < RP_SCR_CHAR_WIDTH - 1; ++i) {
            bits |= (uint32_t) ((BRIDGE_FONT[code - RP_FONT_FIRST][i] >> j) & 1) << i;
        }
        words[0] = (words[0] & ~mask_low) | (bits << shift);
        if (mask_high != 0) {
            words[1] = (words[1] & ~mask_high) | (bits >> (32 - shift));
        }
        words += RP_SCR_WIDTH / 32;
    }
}

// Draws or wipes out the cursor of the console.
BRIDGE_INLINE void bridge_draw_cursor(bridge_t *b, bool is_shown)
{
    if (b->con_col < RP_SCR_COLS) {
        bridge_draw_char(b->scr, b->con_row << 16 | b->con_col << 8 | (is_shown ? '_' : ' '));
    }
}

// Starts the next row of the console; the rows move up when the last one is full.
BRIDGE_INLINE void bridge_start_console_row(bridge_t *b)
{
    uint32_t row_words = RP_SCR_CHAR_HEIGHT * (RP_SCR_WIDTH / 32);

    b->con_col = 0;
    if (++b->con_row == RP_SCR_ROWS) {
        for (uint32_t i = 0; i < RP_SCR_SIZE / 4 - row_words; ++i) {
            b->scr[i] = b->scr[i + row_words];
        }
        for (uint32_t i = RP_SCR_SIZE / 4 - row_words; i < RP_SCR_SIZE / 4; ++i) {
            b->scr[i] = 0;
        }
        b->con_row = RP_SCR_ROWS - 1;
    }
}

// Draws a byte that the CPU has printed, as a terminal does. The rules are
// those of `rp_put_console_byte` of the emulator.
BRIDGE_INLINE void bridge_put_console_byte(bridge_t *b, uint8_t byte)
{
    bridge_draw_cursor(b, false);

    if (b->con_esc == 1) {
        b->con_esc = byte == '[' ? 2 : 0;
    } else if (b->con_esc == 2) {
        if (byte >= 0x40 && byte <= 0x7e) {
            b->con_esc = 0;
        }
    } else if (byte == 0x1b) {
        b->con_esc = 1;
    } else if (byte == '\n') {
        bridge_start_console_row(b);
    } else if (byte == '\r') {
        b->con_col = 0;
    } else if (byte == '\b' || byte == 0x7f) {
        if (b->con_col > 0) {
            --b->con_col;
        }
    } else if (byte >= RP_FONT_FIRST && byte <= RP_FONT_LAST) {
        if (b->con_col == RP_SCR_COLS) {
            bridge_start_console_row(b);
        }
        bridge_draw_char(b->scr, b->con_row << 16 | b->con_col << 8 | byte);
        ++b->con_col;
    }

    bridge_draw_cursor(b, true);
    b->scr_drawn = b->scr_drawn + 1;
}

// Takes the screen from the console for a program that draws: the frame memory starts dark.
BRIDGE_INLINE void bridge_leave_console(bridge_t *b)
{
    if (b->scr_console) {
        b->scr_console = 0;
        for (uint32_t i = 0; i < RP_SCR_SIZE / 4; ++i) {
            b->scr[i] = 0;
        }
    }
}

BRIDGE_INLINE uint32_t bridge_get_irq_stat(bridge_t *b)
{
    uint32_t stat = 0;
    if (b->con_in_put != b->con_in_taken) {
        stat |= RP_IRQ_FLAG_CON;
    }
    if (b->disk_done != b->disk_irq_seen) {
        stat |= RP_IRQ_FLAG_DISK;
    }
    if (b->btn_count != b->btn_irq_seen) {
        stat |= RP_IRQ_FLAG_BTN;
    }

    return stat;
}

// The interrupt line is high while a device requests an interrupt and is enabled.
BRIDGE_INLINE bool bridge_get_irq_line(bridge_t *b)
{
    return (bridge_get_irq_stat(b) & b->irq_enable) != 0;
}

BRIDGE_INLINE uint8_t bridge_access_reg(bridge_t *b, uint32_t addr, bool write, uint32_t time_us, uint32_t *data)
{
    // A running command keeps its sector, its buffer, and itself: writes to the disk's registers are ignored until it ends
    bool is_disk_busy = b->disk_started != b->disk_done;

    // A register of the user page works as its twin of the kernel's page, with two differences below
    bool is_user_reg = addr >= RP_USER_SCR_STAT_ADDR && addr <= RP_USER_SCR_LINE_ADDR;
    if (is_user_reg) {
        BRIDGE_TABLE uint32_t TWINS[] = {
            RP_SCR_STAT_ADDR, RP_SCR_CMD_ADDR, RP_SCR_FRAMES_ADDR, RP_SCR_CHAR_ADDR, RP_SCR_RECT_ADDR,
            RP_SCR_BLIT_ADDR, RP_BTN_STAT_ADDR, RP_BTN_COUNT_ADDR, RP_TIME_ADDR, RP_SCR_MOVE_ADDR, RP_SCR_LINE_ADDR
        };
        addr = TWINS[(addr - RP_USER_SCR_STAT_ADDR) >> 2];
    }

    if (addr == RP_ID_ADDR && !write) {
        *data = RP_ID;
    } else if (addr == RP_SCRATCH_ADDR) {
        if (write) {
            b->scratch = *data;
        } else {
            *data = b->scratch;
        }
    } else if (addr == RP_RAM_SIZE_ADDR && !write) {
        *data = RAM_SIZE;
    } else if (addr == RP_TIME_ADDR && !write) {
        *data = time_us;
    } else if (addr == RP_CON_STAT_ADDR && !write) {
        *data = 0;
        if (b->con_in_put != b->con_in_taken) {
            *data |= RP_CON_FLAG_INPUT;
        }
        if (b->con_out_put - b->con_out_taken != CON_OUT_SIZE) {
            *data |= RP_CON_FLAG_OUTPUT;
        }
    } else if (addr == RP_CON_DATA_ADDR) {
        if (write) {
            uint32_t put = b->con_out_put;
            if (put - b->con_out_taken == CON_OUT_SIZE) {
                ++b->con_out_lost; // the CPU has printed without a look at the status register
            } else {
                b->con_out[put % CON_OUT_SIZE] = (uint8_t) *data;
                BRIDGE_BARRIER();
                b->con_out_put = put + 1;
            }
            if (b->scr_console) {
                bridge_put_console_byte(b, (uint8_t) *data);
            }
        } else if (b->con_in_put != b->con_in_taken) {
            uint32_t taken = b->con_in_taken;
            *data = b->con_in[taken % CON_IN_SIZE];
            b->con_in_taken = taken + 1;
        } else {
            *data = RP_CON_NO_DATA;
        }
    } else if (addr == RP_IRQ_STAT_ADDR) {
        if (write) {
            if ((*data & RP_IRQ_FLAG_DISK) != 0) {
                b->disk_irq_seen = b->disk_done;
            }
            if ((*data & RP_IRQ_FLAG_BTN) != 0) {
                b->btn_irq_seen = b->btn_count;
            }
        } else {
            *data = bridge_get_irq_stat(b);
        }
    } else if (addr == RP_IRQ_ENABLE_ADDR) {
        if (write) {
            b->irq_enable = *data & RP_IRQ_FLAGS;
        } else {
            *data = b->irq_enable;
        }
    } else if (addr == RP_DISK_SEC_ADDR) {
        if (!write) {
            *data = b->disk_sec;
        } else if (!is_disk_busy) {
            b->disk_sec = *data;
        }
    } else if (addr == RP_DISK_BUF_ADDR) {
        if (!write) {
            *data = b->disk_buf;
        } else if (!is_disk_busy) {
            b->disk_buf = *data;
        }
    } else if (addr == RP_DISK_CMD_ADDR) {
        if (!write) {
            *data = is_disk_busy ? b->disk_cmd : 0;
        } else if (!is_disk_busy) {
            b->disk_cmd = *data;
            BRIDGE_BARRIER();
            b->disk_started = b->disk_started + 1; // core 0 does the work and ends the command
        }
    } else if (addr == RP_DISK_STAT_ADDR && !write) {
        if (is_disk_busy) {
            *data = RP_DISK_FLAG_BUSY;
        } else {
            *data = b->disk_error ? RP_DISK_FLAG_ERROR : 0;
        }
    } else if (addr == RP_LOSE_ADDR) {
        if (write) {
            b->lose_cnt = *data;
        } else {
            *data = b->lose_cnt;
        }
    } else if (addr == RP_BTN_STAT_ADDR && !write) {
        *data = b->btn_down ? RP_BTN_FLAG_DOWN : 0;
    } else if (addr == RP_BTN_COUNT_ADDR && !write) {
        *data = b->btn_count;
    } else if (addr == RP_SCR_STAT_ADDR && !write) {
        *data = b->scr_started != b->scr_done ? RP_SCR_FLAG_BUSY : 0;
    } else if (addr == RP_SCR_CMD_ADDR) {
        bool is_scr_busy = b->scr_started != b->scr_done;
        if (!write) {
            *data = is_scr_busy ? RP_SCR_CMD_SHOW : 0;
        } else if (*data == RP_SCR_CMD_CLEAR) {
            bridge_leave_console(b);
            for (uint32_t i = 0; i < RP_SCR_SIZE / 4; ++i) {
                b->scr[i] = 0;
            }
        } else if (*data == RP_SCR_CMD_SHOW) {
            bridge_leave_console(b);
            if (!is_scr_busy) {
                for (uint32_t i = 0; i < RP_SCR_SIZE / 4; ++i) {
                    b->scr_sent[i] = b->scr[i];
                }
                BRIDGE_BARRIER();
                b->scr_started = b->scr_started + 1; // core 0 sends the copy to the screen
            }
        } else if (*data == RP_SCR_CMD_CONSOLE && !is_user_reg) {
            // Only a kernel gives the screen back to the console
            for (uint32_t i = 0; i < RP_SCR_SIZE / 4; ++i) {
                b->scr[i] = 0;
            }
            b->con_row = 0;
            b->con_col = 0;
            b->con_esc = 0;
            bridge_draw_cursor(b, true);
            b->scr_console = 1;
            b->scr_drawn = b->scr_drawn + 1;
        }
    } else if (addr == RP_SCR_FRAMES_ADDR && !write) {
        *data = b->scr_done;
    } else if (addr == RP_SCR_CHAR_ADDR && write) {
        bridge_leave_console(b);
        bridge_draw_char(b->scr, *data);
    } else if (addr == RP_SCR_RECT_ADDR && write) {
        bridge_leave_console(b);
        bridge_draw_rect(b->scr, *data);
    } else if (addr == RP_SCR_MOVE_ADDR && write) {
        b->scr_point = *data;
    } else if (addr == RP_SCR_LINE_ADDR && write) {
        bridge_leave_console(b);
        bridge_draw_line(b->scr, b->scr_point, *data);
        b->scr_point = *data;
    } else if (addr >= RP_SCR_PIC_ADDR && addr < RP_SCR_PIC_ADDR + RP_SCR_PICS * 8) {
        uint32_t *reg = &b->scr_pic[(addr - RP_SCR_PIC_ADDR) >> 3][((addr - RP_SCR_PIC_ADDR) >> 2) & 1];
        if (write) {
            *reg = *data;
        } else {
            *data = *reg;
        }
    } else if (addr == RP_SCR_SRC_ADDR) {
        if (write) {
            b->scr_src = *data;
        } else {
            *data = b->scr_src;
        }
    } else if (addr == RP_SCR_BLIT_ADDR && write) {
        bridge_leave_console(b);
        if (is_user_reg) {
            // A user program draws one of the pictures that the kernel has named
            uint32_t *pic = b->scr_pic[(*data >> 18) & (RP_SCR_PICS - 1)];
            // A picture that the kernel has not named, with both of its addresses, draws nothing
            if (pic[0] != 0 && pic[1] != 0) {
                bridge_draw_picture(b->scr, b->ram, pic[0], pic[1], *data);
            }
        } else {
            bridge_draw_picture(b->scr, b->ram, b->scr_src, 0, *data);
        }
    } else if (addr >= RP_SCR_BASE && addr < RP_SCR_BASE + RP_SCR_SIZE) {
        if (write) {
            bridge_leave_console(b);
            b->scr[(addr - RP_SCR_BASE) >> 2] = *data;
        } else {
            *data = b->scr[(addr - RP_SCR_BASE) >> 2];
        }
    } else {
        return BUS_STAT_ERROR; // no such register, or a write to a read-only one
    }

    return BUS_STAT_OK;
}

/*
    Does one request of the chip and returns the answer. `hdr` is the header
    of the request and `addr` its address, which a fetch of "the next word"
    does not carry. `*data` holds the value of a write and takes the value of
    a read, low byte first. `*is_lost` is set if the test register has asked
    that the chip never get this answer.
*/
BRIDGE_INLINE uint8_t bridge_access(bridge_t *b, uint8_t hdr, uint32_t addr, uint32_t time_us, uint32_t *data, bool *is_lost)
{
    bool write  = (hdr & HDR_WRITE) != 0;
    bool fetch  = (hdr & HDR_FETCH) != 0 && !write;
    bool next   = (hdr & HDR_NEXT) != 0;
    bool kernel = (hdr & HDR_KERNEL) != 0;

    uint32_t size_code = (hdr >> HDR_SIZE_SHIFT) & HDR_SIZE_MASK;
    uint32_t size = (uint32_t) 1 << size_code;

    // The test register counts every access after the write that has set it
    *is_lost = b->lose_cnt > 0 && --b->lose_cnt == 0;

    uint8_t stat = BUS_STAT_OK;
    if (next) {
        if (fetch && b->fetch_valid) {
            addr = b->fetch_addr;
        } else {
            stat = BUS_STAT_BAD_NEXT;
        }
    }

    if (stat != BUS_STAT_OK) {
        // Nothing to do
    } else if (size > 4) {
        stat = BUS_STAT_ERROR;
    } else if ((addr & (size - 1)) != 0) {
        stat = BUS_STAT_MISALIGNED;
    } else if (addr < RAM_SIZE) {
        uint8_t *bytes = b->ram + addr;
        if (write) {
            for (uint32_t i = 0; i < size; ++i) {
                bytes[i] = (uint8_t) (*data >> (8 * i));
            }
        } else {
            uint32_t value = 0;
            for (uint32_t i = 0; i < size; ++i) {
                value |= (uint32_t) bytes[i] << (8 * i);
            }
            *data = value;
        }
    } else if (addr >= BOOT_ROM_BASE && addr < BOOT_ROM_BASE + BOOT_ROM_SIZE) {
        if (write) {
            stat = BUS_STAT_ERROR;
        } else {
            uint32_t value = 0;
            if (addr - BOOT_ROM_BASE < b->rom_len) {
                uint8_t *bytes = b->rom + (addr - BOOT_ROM_BASE);
                for (uint32_t i = 0; i < size; ++i) {
                    value |= (uint32_t) bytes[i] << (8 * i);
                }
            }
            *data = value;
        }
    } else if (addr >= HIGH_BASE && addr - HIGH_BASE < HIGH_SIZE) {
        uint8_t *bytes = b->high + (addr - HIGH_BASE);
        if (write) {
            for (uint32_t i = 0; i < size; ++i) {
                bytes[i] = (uint8_t) (*data >> (8 * i));
            }
        } else {
            uint32_t value = 0;
            for (uint32_t i = 0; i < size; ++i) {
                value |= (uint32_t) bytes[i] << (8 * i);
            }
            *data = value;
        }
    } else if (addr >= RP_BASE && addr < RP_BASE + RP_SIZE) {
        // A user program reaches the page of the screen and the button, and no other
        bool is_user_page = addr >= RP_USER_BASE && addr < RP_USER_BASE + RP_USER_SIZE;
        if ((!kernel && !is_user_page) || fetch) {
            stat = BUS_STAT_PROTECTION;
        } else if (size != 4) {
            stat = BUS_STAT_ERROR;
        } else {
            stat = bridge_access_reg(b, addr, write, time_us, data);
        }
    } else {
        stat = BUS_STAT_ERROR;
    }

    if (fetch) {
        b->fetch_addr = addr + size;
        b->fetch_valid = stat == BUS_STAT_OK && b->fetch_addr != 0; // not past the end of the physical addresses
    }

    return stat;
}

#endif
