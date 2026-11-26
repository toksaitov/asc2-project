/*
    The bridge: the firmware of the microcontroller next to the chip of
    Almurut II: the RP2040 of the bench, or the RP2350 of the demo board of
    Tiny Tapeout.

    The bridge is the chip's link to memory, storage, and peripherals. It
    gives the chip its clock and reset and answers every access of the CPU
    on the bus: RAM, the boot ROM, and the registers of a console, a disk, a
    clock, a screen, and a button (`devices.h`). `asc2-kb/bus-protocol-v1.md`
    describes the bus, and `asc2-kb/bridge-design.md` this program.

    Core 1 serves the bus and does nothing else. It runs from RAM and never
    touches the flash, so a transaction never waits for a flash operation.
    Core 0 does the rest: the USB serial port with the console of the chip
    and the monitor, the work of the disk in the flash, and the watchdog.

    Pins on the bench (`pins.h` has those of the demo board too):
        GP0      out   reset of the chip, low to reset
        GP1      out   clock of the chip: the system clock divided, from a PWM slice
        GP2-GP9  both  bytes of the bus, driven only in the bridge's part of a transaction
        GP10     in    REQ, the strobe of the chip
        GP11     out   ACK, the strobe of the bridge
        GP12     out   IRQ, high while a device requests an interrupt and is enabled
        GP13     out   STOP: while high, the CPU of the chip waits between two instructions
        GP14     out   high while the bridge drives the bytes of the bus
        GP15     in    high while the chip drives them
        GP16     out   clock line of the keyboard that the bridge plays
        GP17     out   data line of that keyboard
        GP18     out   clock of the debug port of the chip
        GP19     out   data input of the debug port
        GP20     in    data output of the debug port
        GP22     in    a push button to ground, for the check of the bench
        GP26     both  SDA of an SSD1306 display, for the same check
        GP27     out   SCL of that display

    Flash: the firmware from the start, the disk in the last 256 KB (64
    sectors of 4 KB).

    What is typed on the serial port goes to the console of the chip, and
    what the chip prints comes back. The bridge keeps a kilobyte of typed
    bytes for a program that does not read yet, so the lines of a whole
    session can be sent at once. Ctrl-A starts one line for the monitor,
    which answers with a line that begins with "ok" or "err":
        load N         takes a firmware image of N bytes, which follow the answer "ok send"
        run            resets the chip and lets it run
        run stopped    resets the chip and holds its CPU before the first instruction
        halt           holds the chip in reset
        stop           holds the CPU after the instruction that it runs
        step [N]       lets a stopped CPU run N instructions (1) and shows the requests of each
        go             lets a stopped CPU run
        regs           reads the debug port of the chip: ip, the instruction, cr5, flags, cr0, the state,
                       and every register: r0 to r7, fp, ksp, lr, usp, cr1 to cr4; of a port that the
                       kernel has locked it shows the lock and nothing else
        key B...       sends the bytes B, given in hex, as a keyboard does
        clock N        sets the clock of the chip to the system clock divided by N; the chip must be held
        peek A [N]     shows N words of RAM or of the boot ROM from the physical address A
        poke A V       writes the word V to RAM at the physical address A; the chip must be held
        stat           numbers about the bus and the devices, and the place of the disk in the flash
        trace          the last requests of the chip with their answers
        faults         the last transactions that the bridge gave up
        watchdog 0|1   turns the watchdog off or on
        oled [TEXT]    shows TEXT on the display, and the button on its last row until the chip shows a frame; with no TEXT a line around the screen
        shift N        tells the display that column N of its memory is the leftmost one on the glass (0 for an SSD1306, 2 for an SH1106)
        button         shows whether the button is down and how often it was pressed
        stepper on|off the button steps the stopped CPU one instruction at a press, and the display shows the address, the instruction, and the flags
        press [MS]     holds the button down for MS milliseconds (100) as a finger does
        frame          shows the frame that went to the display last, a line for a row, `#` for a lit pixel
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/timer.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#define BRIDGE_INLINE    static __force_inline
#define BRIDGE_TABLE     static __not_in_flash("bridge_tables")
#define BRIDGE_BARRIER() __dmb()

#include "devices.h"
#include "pins.h"
#include "rpbus.pio.h"

#define PROGRAM_NAME "bridge"

/* Clocks and Times */

#define SYS_CLOCK_KHZ      150000 // system clock, the one of the RP2350 on a Tiny Tapeout board
#define CHIP_CLOCK_DIV     12     // default divider of the chip's clock: 12.5 MHz
#define RESET_CLOCKS       32     // chip clocks of a reset; the bus asks for 20 at least
#define RESET_EDGE_US      200    // time for the line of the reset to fall or to rise on the demo board
#define MUX_RESET_MS       10     // time of each step of a reset of the demo board's multiplexer
#define MUX_HALF_US        100    // half of the time of a pulse to it
#define QUIET_CLOCKS       4096   // chip clocks without a sign of the chip after which the bridge gives a transaction up
#define ACK_REPAIR_FACTOR  24     // times the quiet time until the bridge sets ACK right again, see `bus_recover`
#define WATCHDOG_US        1000000 // time without a new request after a broken transaction until the chip is reset
#define LOAD_TIMEOUT_US    2000000 // longest wait for the next byte of an image
#define TYPED_WAIT_US      500000  // longest wait of the serial port for a program that does not take its input
#define STOP_TIMEOUT_US    1000000 // longest wait for a stopped CPU to end its instruction
#define STEP_TIMEOUT_US    1000000 // longest wait for the fetch of a step: a halted CPU fetches when an interrupt comes
#define KBD_HALF_BIT_US    40      // half of the time of a bit of the keyboard: 12.5 kHz
#define KBD_GAP_US         2000    // rest between two bytes of the keyboard
#define BUS_ATTEMPTS       4       // number of times that the chip sends a transaction before it gives up
#define DBG_HALF_CLOCKS    6       // chip clocks of half a clock of the debug port; the chip needs 3 to see a level
#define BUTTON_STABLE_US   500     // time that the button keeps a new level before the level counts
#define BUTTON_QUIET_US    20000   // time after it in which the contact may bounce
#define OLED_TIMEOUT_US    20000   // longest time of one message to the display
#define CONSOLE_FRAME_US   40000   // shortest time between two frames of the console on the display

/* Memory */

// The image of a program: the bytes of the boot ROM and then the bytes that
// RAM gets at every reset. An RP2040 has room for 44 KB next to RAM and the frame memory of the screen, an
// RP2350 for the whole ROM.
#if PICO_RP2040
#define IMAGE_SIZE       0xB000
#else
#define IMAGE_SIZE       0x44000
#endif

#define BIN_FMT_MAGIC    0x61736232 /* "asb2" */
#define IMAGE_DATA_ADDR  0x1000     // physical address in RAM of the data section of an image

#define DISK_SECS        64
#define DISK_FLASH_OFF   (PICO_FLASH_SIZE_BYTES - (DISK_SECS * RP_DISK_SEC_SIZE))

// The image that the bridge keeps in its flash and runs at power-up lies below the disk: a sector with a note about it, then its bytes
#define KEPT_FLASH_OFF   (DISK_FLASH_OFF - IMAGE_SIZE - RP_DISK_SEC_SIZE)
#define KEPT_MAGIC       0x6b657074 /* "kept" */

/* PIO */

#define SM               0 // state machine of PIO block 0 that runs the handshake
#define FSTAT_RX_EMPTY   (1u << (PIO_FSTAT_RXEMPTY_LSB + SM))
#define FSTAT_TX_FULL    (1u << (PIO_FSTAT_TXFULL_LSB + SM))
#define CTRL_SM_ENABLE   (1u << (PIO_CTRL_SM_ENABLE_LSB + SM))
#define CTRL_SM_RESTART  (1u << (PIO_CTRL_SM_RESTART_LSB + SM))

// Command words of the handshake, see `rpbus.pio`
#define CMD_RECV         0u
#define CMD_DRIVE(byte)  ((uint32_t) (byte) | 0x100u | (0xffu << 9))

/* Monitor */

#define MONITOR_KEY      0x01 // Ctrl-A
#define MAX_LINE_LEN     64
#define TYPED_SIZE       512    // typed bytes kept for a program that does not read yet
#define FAULT_LOG_SIZE   8      // faults kept for the monitor, a power of 2
#define FAULT_QUIET      0u     // the chip went quiet in the middle of a request
#define FAULT_LATE       1u     // the answer was ready too late
#define FAULT_ANSWER     2u     // the chip went quiet in the middle of the answer
#define FAULT_GUARD      3u     // the chip drove the bus when the bridge was about to answer
#define FAULT_IDLE_GUARD 4u     // the same while the bridge waited for a header
#define TRACE_SIZE       32   // requests that `trace` shows
#define MAX_STEPS        256  // instructions of one `step`
#define MAX_KEYS         16   // bytes of one `key`
#define KBD_FRAME_BITS   11   // a start bit, 8 bits of the byte, a parity bit, a stop bit
#define DBG_BITS         643  // bits of the debug port
#define DBG_WORDS        ((DBG_BITS + 31) / 32)
#define DBG_LOCKED       640  // the bit that is set while the kernel has locked the port

/* The Display */

#define OLED_I2C         i2c1
#define OLED_BAUD        400000
#define OLED_ADDR        0x3c  // the usual address of an SSD1306; some boards have 0x3d
#define OLED_ADDR_OTHER  0x3d
#define OLED_RAM_WIDTH   132   // columns of the memory of an SH1106; an SSD1306 has 128
#define OLED_SHIFT       2     // first column on the glass: 0 for an SSD1306, 2 for an SH1106, which the bench has
#define OLED_RESET_US    100   // length of the reset pulse, and of the wait after it
#define OLED_HALF_CYCLES 18    // clocks of the Pico in half a clock of the display: 240 ns for a whole one, and an SH1106 needs 250 with the instructions around it
#define OLED_PAGES       8     // bands of 8 rows
#define OLED_COMMAND     0x00  // first byte of a message with commands
#define OLED_DATA        0x40  // first byte of a message with pixels

typedef struct bin_fmt_header
{
    uint32_t magic;
    uint32_t data_sec_size;
    uint32_t text_sec_size;
} bin_fmt_header_t;

typedef struct bus_stats
{
    volatile uint32_t requests;    // transactions answered
    volatile uint32_t repeats;     // requests that repeated the one before and got its kept answer
    volatile uint32_t recoveries;  // transactions given up, for any of the reasons below
    volatile uint32_t quiet;       // the chip went quiet in the middle of a request
    volatile uint32_t late;        // the answer was ready too late to be sent safely
    volatile uint32_t lost;        // answers held back on the order of the test register
    volatile uint32_t guard;       // the chip said that it drives when the bridge was about to
    volatile uint32_t ack_repairs; // ACK set equal to REQ after an unanswered change of REQ
} bus_stats_t;

typedef struct trace_entry
{
    uint32_t hdr;
    uint32_t addr;
    uint32_t data;     // value written, or value read
    uint32_t stat;     // answer
} trace_entry_t;

// A transaction that the bridge gave up, as the bridge saw it
typedef struct fault_entry
{
    uint32_t request; // number of requests served before it
    uint32_t kind;    // one of the `FAULT_` numbers
    uint32_t hdr;
    uint32_t addr;
    uint32_t wdata;
    uint32_t done;    // bytes after the header that came, or steps of the answer that the chip took
    uint32_t wanted;
    uint32_t pins;    // levels of the pins at that moment
    uint32_t next_hdr; // the request that followed
    uint32_t next_addr;
    uint32_t next_wdata;
    uint32_t has_next;
} fault_entry_t;

static uint8_t ram[RAM_SIZE] __attribute__((aligned(4)));
static uint8_t image[IMAGE_SIZE] __attribute__((aligned(4)));
static uint8_t high[HIGH_SIZE] __attribute__((aligned(4)));
static uint32_t image_data_off;  // offset of the data section in `image`
static uint32_t image_data_size;
static bool has_image;

static bridge_t bridge;

/* Shared by the Cores */

static volatile bool is_server_running;
static volatile uint32_t quiet_us;      // `QUIET_CLOCKS` in microseconds
static volatile uint32_t ack_repair_us;
static volatile uint32_t time_base;     // timer value at the reset of the chip

static volatile bool is_broken;       // a transaction was given up, and no request has come since
static volatile uint32_t broken_time; // timer value when it was

// Core 0 raises `flash_req` before it turns the flash off, and core 1 copies
// it to `flash_ack` at a place where it runs from RAM
static volatile uint32_t flash_req;
static volatile uint32_t flash_ack;

// Core 0 sets `is_step_armed` for a step, and core 1 raises STOP at the next
// instruction fetch and clears it
static volatile bool is_step_armed;
static volatile uint32_t fetch_cnt; // instruction fetches since the reset of the chip

// Core 1 sets `is_button_reset` when it sees the line of the reset low: the reset button of the demo board is down
static volatile bool is_button_reset;

static bus_stats_t bus_stats;
static trace_entry_t trace[TRACE_SIZE];
static volatile uint32_t trace_cnt;
static fault_entry_t faults[FAULT_LOG_SIZE];
static volatile uint32_t fault_cnt;

// Instructions that core 1 gives the state machine when it recovers. Core 0
// encodes them, so that core 1 calls nothing in the flash.
static uint32_t pio_offset;
static uint32_t inst_clear_osr;
static uint32_t inst_release;
static uint32_t inst_jmp_rise;
static uint32_t inst_jmp_fall;
static uint32_t inst_ack_low;
static uint32_t inst_ack_high;

/* Core 0 Only */

static uint32_t chip_clock_div;
static bool is_watchdog_on = true;
static uint32_t watchdog_resets;
static bool is_stopped;           // the monitor holds STOP high
static uint8_t __scratch_x("typed") typed[TYPED_SIZE]; // in the free half of the bank of core 1's stack: typed bytes that wait for room in the console's queue, at their number modulo the size
static uint32_t typed_put;        // number of bytes put there
static uint32_t typed_taken;      // number of bytes that have gone on to the console
static uint32_t typed_lost;       // typed bytes dropped because the program did not take the earlier ones
static uint32_t disk_reads;
static uint32_t disk_writes;
static uint32_t disk_errors;
static bool is_oled_started;      // the display has taken its first commands
static uint8_t oled_addr;
static bool is_oled_serial;       // the display takes its bytes over a clock and a data line, with no answer
static uint32_t oled_shift = OLED_SHIFT; // column of the display's memory that is the leftmost one on the glass
static uint32_t oled_frames;      // frames sent to the display
static bool is_monitor_frame;     // the display shows that picture
static bool is_button_down;       // the level of the button that has counted last
static bool is_button_valid;      // a level has counted
static bool button_last;          // the level read last
static uint32_t button_since;     // timer value when that level began
static uint32_t button_counted;   // timer value when a level counted last
static uint32_t button_presses;
static bool is_stepper_on;        // the button steps the stopped CPU, and the display shows where it is
static uint32_t stepper_steps;    // instructions that the button has run
static bool is_press_played;      // the monitor holds the button down
static uint32_t press_end;        // timer value when it lets go

/*
    Core 1: The Bus Server

    Everything in this part runs from RAM and calls nothing in the flash: no
    `switch`, no division, no library call. Core 0 turns the flash off while
    it erases and programs a sector of the disk.
*/

static __force_inline void flash_sync(void)
{
    uint32_t req = flash_req;
    if (req != flash_ack) {
        __dmb();
        flash_ack = req;
    }
}

static __force_inline void irq_line_update(void)
{
    if (bridge_get_irq_line(&bridge)) {
        sio_hw->gpio_set = IRQ_MASK;
    } else {
        sio_hw->gpio_clr = IRQ_MASK;
    }
}

/*
    The reset button of the demo board is down, or was for a moment. The chip
    starts anew when it goes up and drives the bus 15 clocks later, so the
    state machine stops and releases the bus at once. ACK keeps its level,
    which the chip takes as the start. Core 0 resets the machine when the
    button is up, and this core waits until then.
*/
static void __not_in_flash_func(button_reset)(void)
{
    hw_clear_bits(&pio0_hw->ctrl, CTRL_SM_ENABLE);
    pio0_hw->sm[SM].instr = inst_clear_osr;
    pio0_hw->sm[SM].instr = inst_release; // the pins of the bus become inputs
    is_button_reset = true;

    for (;;) {
        flash_sync();
    }
}

/*
    Gives a transaction up. The state machine stops and releases the bus at
    once, and ACK keeps its level. When REQ has been still for the quiet
    time, the handshake starts again at the level that REQ has, with a
    receive waiting, so that the next change of REQ is taken as a header.
    The chip repeats a request that got no answer.

    The state machine answers an event by setting ACK to the new level of
    REQ, so between transactions the two are equal. If the bridge gave up
    without an answer to the last change of REQ, they differ, and the first
    answer after the restart would set ACK to the level it has: the chip
    would see nothing. ACK is then set equal to REQ first, in the middle of
    the time in which the chip does not listen: the chip waits 65535 clocks
    for the missing answer and then stays quiet for 65535 clocks, in which
    it takes the level of ACK as it is. `ACK_REPAIR_FACTOR` quiet times
    after the last change of REQ is that middle. A change of REQ during the
    wait is one more attempt without an answer and starts the wait again.
*/
static void __not_in_flash_func(bus_recover)(void)
{
    broken_time = timer_hw->timerawl;
    __dmb();
    is_broken = true;

    hw_clear_bits(&pio0_hw->ctrl, CTRL_SM_ENABLE);
    pio0_hw->sm[SM].instr = inst_clear_osr;
    pio0_hw->sm[SM].instr = inst_release; // the pins of the bus become inputs

    uint32_t level = REQ_LEVEL();
    uint32_t still_since = timer_hw->timerawl;
    for (;;) {
        uint32_t req = REQ_LEVEL();
        uint32_t now = timer_hw->timerawl;
        if (CHIP_IS_RESET()) {
            button_reset();
        }
        if (req != level) {
            level = req;
            still_since = now;
        }

        uint32_t ack = (sio_hw->gpio_in >> PIN_ACK) & 1u;
        if (ack == level) {
            if (now - still_since >= quiet_us) {
                break;
            }
        } else if (now - still_since >= ack_repair_us) {
            pio0_hw->sm[SM].instr = level ? inst_ack_high : inst_ack_low;
            ++bus_stats.ack_repairs;
            break;
        }

        flash_sync();
    }

    // The state machine starts anew with empty FIFOs
    hw_set_bits(&pio0_hw->ctrl, CTRL_SM_RESTART);
    hw_xor_bits(&pio0_hw->sm[SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);
    hw_xor_bits(&pio0_hw->sm[SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);
    pio0_hw->sm[SM].instr = level ? inst_jmp_fall : inst_jmp_rise;
    hw_set_bits(&pio0_hw->ctrl, CTRL_SM_ENABLE);
    pio0_hw->txf[SM] = CMD_RECV;

    ++bus_stats.recoveries;
}

// Keeps what the bridge knows about a transaction that it gives up
static __force_inline void fault_note(uint32_t kind, uint32_t hdr, uint32_t addr, uint32_t wdata, uint32_t done, uint32_t wanted)
{
    fault_entry_t *entry = &faults[fault_cnt & (FAULT_LOG_SIZE - 1u)];
    entry->request = bus_stats.requests;
    entry->kind = kind;
    entry->hdr = hdr;
    entry->addr = addr;
    entry->wdata = wdata;
    entry->done = done;
    entry->wanted = wanted;
    entry->pins = PIN_LEVELS();
    entry->has_next = 0;
    fault_cnt = fault_cnt + 1u;
}

// The state machine found the chip driving the bus when the bridge was about
// to, and has stopped with its outputs off. The request gets no answer, and
// the chip repeats it.
static void __not_in_flash_func(guard_recover)(void)
{
    ++bus_stats.guard;
    pio0_hw->irq = 1u;
    bus_recover();
}

// Waits for the header of the next request. The interrupt line follows the
// devices meanwhile, so that a byte typed while the CPU sleeps wakes it.
static __force_inline uint32_t wait_for_header(void)
{
    while (pio0_hw->fstat & FSTAT_RX_EMPTY) {
        flash_sync();
        irq_line_update();
        if (CHIP_IS_RESET()) {
            button_reset();
        }
        if (pio0_hw->irq & 1u) {
            fault_note(FAULT_IDLE_GUARD, 0, 0, 0, 0, 0);
            guard_recover();
        }
    }

    return pio0_hw->rxf[SM] & 0xffu;
}

static void __not_in_flash_func(server)(void)
{
    uint32_t cmds[7];

    // The last request with its answer. A request that is equal to it byte
    // for byte is a repeat: the chip never got the answer. It gets the same
    // answer, and the access is not done twice.
    bool has_last = false;
    uint32_t last_hdr = 0;
    uint32_t last_addr = 0;
    uint32_t last_wdata = 0;
    uint32_t last_stat = 0;
    uint32_t last_rdata = 0;
    uint32_t last_lost_left = 0; // attempts at the last request that still get no answer, as the test register asked

    pio0_hw->txf[SM] = CMD_RECV; // for the first header

    for (;;) {
        uint32_t hdr = wait_for_header();
        uint32_t last_sign = timer_hw->timerawl; // time of the last sign of the chip
        is_broken = false;

        bool write = (hdr & HDR_WRITE) != 0;
        bool next = (hdr & HDR_NEXT) != 0;
        uint32_t size = 1u << ((hdr >> HDR_SIZE_SHIFT) & HDR_SIZE_MASK);

        /* The address and the bytes of a write */

        uint32_t addr_cnt = next ? 0u : 4u;
        uint32_t recv_cnt = addr_cnt + (write ? size : 0u);
        uint32_t addr = 0;
        uint32_t wdata = 0;

        uint32_t asked = 0;
        uint32_t got = 0;
        bool is_quiet = false;
        while (got < recv_cnt) {
            uint32_t fstat = pio0_hw->fstat;
            if (asked < recv_cnt && !(fstat & FSTAT_TX_FULL)) {
                pio0_hw->txf[SM] = CMD_RECV;
                ++asked;
            }

            if (!(fstat & FSTAT_RX_EMPTY)) {
                uint32_t byte = pio0_hw->rxf[SM] & 0xffu;
                if (got < addr_cnt) {
                    addr |= byte << (8u * got);
                } else if (got - addr_cnt < 4u) {
                    wdata |= byte << (8u * (got - addr_cnt));
                }
                ++got;
                last_sign = timer_hw->timerawl;
            } else {
                flash_sync();
                if (CHIP_IS_RESET()) {
                    button_reset();
                }
                if (timer_hw->timerawl - last_sign > quiet_us) {
                    is_quiet = true;
                    break;
                }
            }
        }
        if (is_quiet) {
            ++bus_stats.quiet;
            fault_note(FAULT_QUIET, hdr, addr, wdata, got, recv_cnt);
            bus_recover();
            continue;
        }
        if (fault_cnt != 0 && !faults[(fault_cnt - 1u) & (FAULT_LOG_SIZE - 1u)].has_next) {
            fault_entry_t *entry = &faults[(fault_cnt - 1u) & (FAULT_LOG_SIZE - 1u)];
            entry->next_hdr = hdr;
            entry->next_addr = addr;
            entry->next_wdata = wdata;
            entry->has_next = 1;
        }

        /* The access. The next change of REQ hands the bus over, and the state machine holds the chip there. */

        uint32_t stat;
        uint32_t data;
        bool is_lost;
        if (has_last && hdr == last_hdr && addr == last_addr && wdata == last_wdata) {
            stat = last_stat;
            data = last_rdata;
            is_lost = last_lost_left > 0;
            if (is_lost && --last_lost_left == 0) {
                has_last = false; // after the last attempt the same request is a new transaction
            }
            ++bus_stats.repeats;
        } else {
            data = wdata;
            stat = bridge_access(&bridge, (uint8_t) hdr, addr, timer_hw->timerawl - time_base, &data, &is_lost);
            irq_line_update();

            has_last = true;
            last_hdr = hdr;
            last_addr = addr;
            last_wdata = wdata;
            last_stat = stat;
            last_rdata = data;
            last_lost_left = is_lost ? BUS_ATTEMPTS - 1u : 0;

            trace_entry_t *entry = &trace[trace_cnt & (TRACE_SIZE - 1u)];
            entry->hdr = hdr;
            entry->addr = next ? bridge.fetch_addr - size : addr;
            entry->data = data;
            entry->stat = stat;
            trace_cnt = trace_cnt + 1u;

            // A step waits for this fetch. STOP is high before the answer
            // goes out, so the CPU runs the instruction and waits again.
            if ((hdr & (HDR_WRITE | HDR_FETCH)) == HDR_FETCH) {
                fetch_cnt = fetch_cnt + 1u;
                if (is_step_armed) {
                    sio_hw->gpio_set = STOP_MASK;
                    is_step_armed = false;
                }
            }
        }

        // The test register has asked that the chip never get this answer, nor the answer to a repeat
        if (is_lost) {
            ++bus_stats.lost;
            bus_recover();
            continue;
        }

        // A chip that has waited this long may give up and start again at any moment, and an answer now could meet its next header
        if (timer_hw->timerawl - last_sign > quiet_us) {
            ++bus_stats.late;
            fault_note(FAULT_LATE, hdr, addr, wdata, 0, 0);
            bus_recover();
            continue;
        }

        /*
            The answer: the status with the sequence bit of the header and
            its opposite, the bytes of a read that went well, the release of
            the bus, and a receive for the next header. The state machine
            pushes a sample for every one of these events. All are thrown
            away except the next header.
        */

        uint32_t read_cnt = (stat == BUS_STAT_OK && !write) ? size : 0u;
        uint32_t cmds_cnt = 0;
        cmds[cmds_cnt++] = CMD_DRIVE(((hdr & HDR_SEQ) ? 0x80u : 0x40u) | stat);
        for (uint32_t i = 0; i < read_cnt; ++i) {
            cmds[cmds_cnt++] = CMD_DRIVE((data >> (8u * i)) & 0xffu);
        }
        cmds[cmds_cnt++] = CMD_RECV; // the release
        cmds[cmds_cnt++] = CMD_RECV; // the next header

        uint32_t sent = 0;
        uint32_t to_drop = read_cnt + 2u;
        bool is_guarded = false;
        while (sent < cmds_cnt || to_drop != 0) {
            uint32_t fstat = pio0_hw->fstat;
            if (sent < cmds_cnt && !(fstat & FSTAT_TX_FULL)) {
                pio0_hw->txf[SM] = cmds[sent++];
            }

            if (to_drop != 0) {
                if (!(fstat & FSTAT_RX_EMPTY)) {
                    (void) pio0_hw->rxf[SM];
                    --to_drop;
                    last_sign = timer_hw->timerawl;
                } else if (timer_hw->timerawl - last_sign > quiet_us) {
                    is_quiet = true;
                    break;
                }
            }

            if (pio0_hw->irq & 1u) {
                is_guarded = true;
                break;
            }
            if (CHIP_IS_RESET()) {
                button_reset();
            }

            flash_sync();
        }
        if (is_guarded) {
            fault_note(FAULT_GUARD, hdr, addr, wdata, read_cnt + 2u - to_drop, read_cnt + 2u);
            guard_recover();
            continue;
        }
        if (is_quiet) {
            ++bus_stats.quiet;
            fault_note(FAULT_ANSWER, hdr, addr, wdata, read_cnt + 2u - to_drop, read_cnt + 2u);
            bus_recover();
            continue;
        }

        ++bus_stats.requests;
    }
}

/* Core 0: The Flash */

// Takes the flash from core 1, which may be serving the bus
static void flash_begin(void)
{
    flash_req = 1;
    __dmb();
    if (is_server_running) {
        while (!flash_ack) {
            tight_loop_contents();
        }
    }
    __dmb();
}

static void flash_end(void)
{
    __dmb();
    flash_req = 0;
    if (is_server_running) {
        while (flash_ack) {
            tight_loop_contents();
        }
    }
}

static void flash_write_sector(uint32_t off, const uint8_t *bytes)
{
    flash_begin();
    uint32_t irqs = save_and_disable_interrupts();
    flash_range_erase(off, RP_DISK_SEC_SIZE);
    flash_range_program(off, bytes, RP_DISK_SEC_SIZE);
    restore_interrupts(irqs);
    flash_end();
}

// Does the work of a disk command: copies a sector between the flash and the
// buffer in RAM. The bus is served meanwhile.
static void disk_work(void)
{
    if (!bridge_disk_is_busy(&bridge)) {
        return;
    }

    bool is_valid = bridge_disk_is_valid(&bridge, DISK_SECS);
    if (is_valid) {
        uint32_t off = DISK_FLASH_OFF + (bridge.disk_sec * RP_DISK_SEC_SIZE);
        const uint8_t *sec = (const uint8_t *) (XIP_NOCACHE_NOALLOC_BASE + off);
        uint8_t *buf = ram + bridge.disk_buf;
        if (bridge.disk_cmd == RP_DISK_CMD_READ) {
            memcpy(buf, sec, RP_DISK_SEC_SIZE);
            ++disk_reads;
        } else {
            flash_write_sector(off, buf);
            is_valid = memcmp(sec, buf, RP_DISK_SEC_SIZE) == 0; // the flash did not take the bytes
            ++disk_writes;
        }
    }
    if (!is_valid) {
        ++disk_errors;
    }

    bridge_disk_finish(&bridge, is_valid);
}

/* Core 0: The Pins, the Clock, and the Reset */

static void pins_init(void)
{
    for (uint32_t pin = PINS_FIRST; pin < PINS_END; ++pin) {
        gpio_init(pin);
        gpio_set_pulls(pin, false, true);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
    }

#if TT_DEMOBOARD_V3
    // The reset lies beside the other pins, and the board pulls it up with its button or its inverter
    gpio_init(PIN_RST);
    gpio_disable_pulls(PIN_RST);
    gpio_set_drive_strength(PIN_RST, GPIO_DRIVE_STRENGTH_8MA);

    // The input that the chip does not use gets a level: a pin of the first
    // RP2350 that nobody drives can rest between low and high (erratum E9)
    gpio_put(PIN_FREE, false);
    gpio_set_dir(PIN_FREE, GPIO_OUT);
#endif

    // STOP is low, and the two lines of the keyboard rest high
    gpio_put(PIN_STOP, false);
    gpio_set_dir(PIN_STOP, GPIO_OUT);
    gpio_put(PIN_KBD_CLK, true);
    gpio_set_dir(PIN_KBD_CLK, GPIO_OUT);
    gpio_put(PIN_KBD_DATA, true);
    gpio_set_dir(PIN_KBD_DATA, GPIO_OUT);

    // The two inputs of the debug port rest low, and its output is read
    gpio_put(PIN_DBG_CK, false);
    gpio_set_dir(PIN_DBG_CK, GPIO_OUT);
    gpio_put(PIN_DBG_DI, false);
    gpio_set_dir(PIN_DBG_DI, GPIO_OUT);

    // The button joins its pin to ground
    gpio_init(PIN_BUTTON);
    gpio_set_pulls(PIN_BUTTON, true, false);

    // The lines of the display; `oled_start` gives the clock and the data
    // line their function. A board with a display has its own pull-ups too.
    i2c_init(OLED_I2C, OLED_BAUD);
    gpio_set_pulls(PIN_OLED_SDA, true, false);
    gpio_set_pulls(PIN_OLED_SCL, true, false);
    gpio_init(PIN_OLED_RST);
    gpio_put(PIN_OLED_RST, true);
    gpio_set_dir(PIN_OLED_RST, GPIO_OUT);
    gpio_init(PIN_OLED_DC);
    gpio_put(PIN_OLED_DC, false);
    gpio_set_dir(PIN_OLED_DC, GPIO_OUT);
}

static void clock_start(uint32_t div)
{
    uint32_t slice = pwm_gpio_to_slice_num(PIN_CLK);

    pwm_set_enabled(slice, false);
    pwm_set_clkdiv(slice, 1.0f);
    pwm_set_wrap(slice, (uint16_t) (div - 1));
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_CLK), (uint16_t) (div / 2));
    gpio_set_function(PIN_CLK, GPIO_FUNC_PWM);
    pwm_set_counter(slice, 0);
    pwm_set_enabled(slice, true);

    chip_clock_div = div;

    // The times that the bus counts in clocks of the chip, in microseconds and rounded up
    uint64_t clock_hz = clock_get_hz(clk_sys) / div;
    quiet_us = (uint32_t) ((((uint64_t) QUIET_CLOCKS * 1000000u) + clock_hz - 1u) / clock_hz);
    ack_repair_us = ACK_REPAIR_FACTOR * quiet_us;
}

#if TT_DEMOBOARD_V3

static void chip_clock_stop(void)
{
    gpio_init(PIN_CLK);
    gpio_put(PIN_CLK, false);
    gpio_set_dir(PIN_CLK, GPIO_OUT);
    pwm_set_enabled(pwm_gpio_to_slice_num(PIN_CLK), false);
}

/*
    Chooses the chip among the designs of the shuttle: a reset of the
    multiplexer and one pulse for each design that comes before ours, with
    the chip held in reset. A design that is not chosen has no clock, and
    its inputs are low. The board pulls the three lines up, and the carrier
    of the chip pulls the enable and the reset down with 1 kOhm.
*/
static void design_select(void)
{
    static const uint8_t pins[] = {PIN_CTRL_ENA, PIN_CTRL_RST, PIN_CTRL_INC};
    for (uint32_t i = 0; i < sizeof(pins); ++i) {
        gpio_init(pins[i]);
        gpio_disable_pulls(pins[i]);
        gpio_set_drive_strength(pins[i], GPIO_DRIVE_STRENGTH_8MA);
        gpio_put(pins[i], false);
        gpio_set_dir(pins[i], GPIO_OUT);
    }

    sleep_ms(MUX_RESET_MS);
    gpio_put(PIN_CTRL_RST, true);
    sleep_ms(MUX_RESET_MS);
    for (uint32_t i = 0; i < TT_DESIGN; ++i) {
        gpio_put(PIN_CTRL_INC, true);
        busy_wait_us(MUX_HALF_US);
        gpio_put(PIN_CTRL_INC, false);
        busy_wait_us(MUX_HALF_US);
    }
    gpio_put(PIN_CTRL_ENA, true);
    sleep_ms(MUX_RESET_MS);
}

#endif

static uint32_t chip_clock_hz(void)
{
    return clock_get_hz(clk_sys) / chip_clock_div;
}

static void server_setup(void)
{
    pio_sm_config config = rpbus_program_get_default_config(pio_offset);

    sm_config_set_in_pins(&config, PIN_UIO);
    sm_config_set_out_pins(&config, PIN_UIO, 8);
    sm_config_set_set_pins(&config, PIN_ACK, 1);
    sm_config_set_sideset_pins(&config, PIN_BRIDGE_DRIVES);
    sm_config_set_jmp_pin(&config, PIN_CHIP_DRIVES);
    sm_config_set_in_shift(&config, false, true, 8);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_clkdiv(&config, 1.0f);
    pio_sm_init(pio0, SM, pio_offset, &config);

    // The bus pins are inputs, and ACK and the pin that tells of the bridge's driving are low
    uint64_t mask = UIO_MASK | ACK_MASK | BRIDGE_DRIVES_MASK;
    pio_sm_set_pins_with_mask64(pio0, SM, 0, mask);
    pio_sm_set_pindirs_with_mask64(pio0, SM, ACK_MASK | BRIDGE_DRIVES_MASK, mask);
    for (uint32_t pin = PIN_UIO; pin < PIN_UIO + 8; ++pin) {
        pio_gpio_init(pio0, pin);
        gpio_set_pulls(pin, BUS_PULL_UP, !BUS_PULL_UP);
    }
    pio_gpio_init(pio0, PIN_ACK);
    pio_gpio_init(pio0, PIN_BRIDGE_DRIVES);
    pio0_hw->irq = 1u;

    inst_clear_osr = pio_encode_mov(pio_osr, pio_null);
    inst_release = pio_encode_out(pio_pindirs, 8) | pio_encode_sideset_opt(1, 0);
    inst_jmp_rise = pio_encode_jmp(pio_offset + rpbus_offset_rise);
    inst_jmp_fall = pio_encode_jmp(pio_offset + rpbus_offset_fall);
    inst_ack_low = pio_encode_set(pio_pins, 0);
    inst_ack_high = pio_encode_set(pio_pins, 1);
}

// Holds the chip in reset. The bus server stops, and the bridge drives no pin of the bus.
static void chip_halt(void)
{
    gpio_put(PIN_RST, false);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    multicore_reset_core1();
    is_server_running = false;
    is_broken = false;
    pio_sm_set_enabled(pio0, SM, false);

    for (uint32_t pin = PIN_UIO; pin < PIN_UIO + 8; ++pin) {
        gpio_init(pin);
        gpio_set_pulls(pin, BUS_PULL_UP, !BUS_PULL_UP);
    }
    gpio_init(PIN_ACK);
    gpio_put(PIN_ACK, false);
    gpio_set_dir(PIN_ACK, GPIO_OUT);
    gpio_init(PIN_BRIDGE_DRIVES);
    gpio_put(PIN_BRIDGE_DRIVES, false);
    gpio_set_dir(PIN_BRIDGE_DRIVES, GPIO_OUT);
    gpio_put(PIN_IRQ, false);
    gpio_set_dir(PIN_IRQ, GPIO_OUT);

    is_step_armed = false;
    is_button_reset = false;
    gpio_put(PIN_STOP, false);
    is_stopped = false;
    is_stepper_on = false;

    flash_req = 0;
    flash_ack = 0;
}

/*
    Resets the chip and lets it run. The reset lasts `RESET_CLOCKS` clocks,
    during which the bridge neither drives the bus nor listens: the chip's
    own outputs have their levels of the reset only after some clocks of it.
    The devices start anew, and RAM keeps its bytes except for the data
    section of the image, which is put in place again. With `is_held` set,
    STOP is high from the start, and the CPU waits before its first
    instruction.
*/
static void chip_run(bool is_held)
{
    chip_halt();

    bridge_reset(&bridge);
    memcpy(ram + IMAGE_DATA_ADDR, image + image_data_off, image_data_size);
    memset(&bus_stats, 0, sizeof(bus_stats));
    trace_cnt = 0;
    fault_cnt = 0;
    fetch_cnt = 0;

    gpio_put(PIN_STOP, is_held);
    is_stopped = is_held;

#if TT_DEMOBOARD_V3
    // One version of the board has 100 nF on the line of the reset, which
    // falls slowly. The clocks of the reset count from its low level.
    busy_wait_us(RESET_EDGE_US);
#endif

    busy_wait_us((uint32_t) ((((uint64_t) RESET_CLOCKS * 1000000u) / chip_clock_hz()) + 1u));

#if TT_DEMOBOARD_V3
    // The line of the reset rises slowly on one version of the board. The
    // clock stands meanwhile, so that the filter of the chip sees the end of
    // it only. Then the bridge lets go of the line, which the board holds
    // high and its button can pull low.
    chip_clock_stop();
    gpio_put(PIN_RST, true);
    busy_wait_us(RESET_EDGE_US);
    gpio_set_dir(PIN_RST, GPIO_IN);
#endif

    server_setup();
    pio_sm_set_enabled(pio0, SM, true);
    time_base = timer_hw->timerawl;
    is_server_running = true;
    multicore_launch_core1(server);

#if TT_DEMOBOARD_V3
    clock_start(chip_clock_div);
#else
    gpio_put(PIN_RST, true);
#endif
}

// Resets the machine after the reset button of the demo board, when the button is up again
static void button_reset_work(void)
{
    if (!is_button_reset || !is_server_running || CHIP_IS_RESET()) {
        return;
    }

    printf("\n" PROGRAM_NAME ": the reset button was pressed\n");
    chip_run(false);
}

// Resets a chip that has stopped in the middle of its work: a chip in order
// repeats a request that got no answer well within the time of the watchdog.
// A CPU that the monitor holds sends nothing and is left alone.
static void watchdog_work(void)
{
    if (!is_watchdog_on || !is_server_running || !is_broken || is_stopped) {
        return;
    }
    if (timer_hw->timerawl - broken_time < WATCHDOG_US) {
        return;
    }

    ++watchdog_resets;
    printf("\n" PROGRAM_NAME ": the chip went quiet in the middle of a transaction and is reset\n");
    chip_run(false);
}

/* Core 0: Stopping and Stepping */

/*
    Waits until a CPU that sees STOP high has ended its instruction: the
    chip has then sent nothing for two quiet times of the bus, and no
    transaction waits to be repeated. An instruction leaves a few clocks
    between its requests, and a quiet time is thousands. Returns false if
    the chip goes on for `STOP_TIMEOUT_US`.
*/
static bool chip_wait_still(void)
{
    uint32_t start = timer_hw->timerawl;
    uint32_t requests = trace_cnt;
    uint32_t still_since = start;
    for (;;) {
        uint32_t now = timer_hw->timerawl;
        uint32_t cnt = trace_cnt;
        if (cnt != requests || is_broken) {
            requests = cnt;
            still_since = now;
        } else if (now - still_since > 2u * quiet_us) {
            return true;
        }

        if (now - start > STOP_TIMEOUT_US) {
            return false;
        }
    }
}

// Shows a request as a step shows it: what it was, its address, and its value.
static void print_request(const trace_entry_t *entry)
{
    uint32_t hdr = entry->hdr;
    uint32_t size = 1u << ((hdr >> HDR_SIZE_SHIFT) & HDR_SIZE_MASK);
    const char *kind = (hdr & HDR_WRITE) ? "write" : ((hdr & HDR_FETCH) ? "fetch" : "read");

    printf("%s %08lx: ", kind, (unsigned long) entry->addr);
    if (entry->stat == BUS_STAT_OK) {
        printf("%0*lx\n", (int) (2u * size), (unsigned long) entry->data);
    } else {
        printf("refused with the answer %lu\n", (unsigned long) entry->stat);
    }
}

/*
    One clock of the debug port: a rise with the data input high selects
    bit 0, and a rise with the input low the next bit. The chip takes both
    lines through two flops of its own clock, so each level stays for some
    clocks of the chip.
*/
static void debug_clock(bool di)
{
    uint32_t half_us = (DBG_HALF_CLOCKS * 1000000u / chip_clock_hz()) + 1u;

    gpio_put(PIN_DBG_DI, di);
    busy_wait_us(half_us);
    gpio_put(PIN_DBG_CK, true);
    busy_wait_us(half_us);
    gpio_put(PIN_DBG_CK, false);
    busy_wait_us(half_us);
}

// Selects bit 0 of the debug port. The chip empties its instruction cache then.
static void debug_restart(void)
{
    debug_clock(true);
    gpio_put(PIN_DBG_DI, false);
}

/*
    Lets a stopped CPU run one instruction and shows its requests. STOP goes
    low, and core 1 raises it again when the fetch has come, before it sends
    the answer. A CPU that takes an interrupt first reads the vector of the
    interrupt and then fetches the first instruction of the handler; the
    step shows both. Returns NULL, or what went wrong.
*/
static const char *chip_step(bool is_quiet)
{
    debug_restart(); // the cache is empty, so the instruction is a fetch on the bus

    uint32_t first = trace_cnt;
    uint32_t fetches = fetch_cnt;

    is_step_armed = true;
    __dmb();
    gpio_put(PIN_STOP, false);

    uint32_t start = timer_hw->timerawl;
    while (is_step_armed && timer_hw->timerawl - start < STEP_TIMEOUT_US) {
        tight_loop_contents();
    }

    // Core 1 has raised STOP already if a fetch came
    gpio_put(PIN_STOP, true);
    is_step_armed = false;
    bool is_still = chip_wait_still();

    uint32_t cnt = trace_cnt;
    for (uint32_t i = (cnt - first > TRACE_SIZE) ? cnt - TRACE_SIZE : first; i != cnt && !is_quiet; ++i) {
        print_request(&trace[i & (TRACE_SIZE - 1u)]);
    }

    if (fetch_cnt == fetches) {
        return "no instruction came; the CPU is halted and waits for an interrupt";
    }
    if (!is_still) {
        return "the chip goes on with STOP high";
    }

    return NULL;
}

/* Core 0: The Keyboard */

/*
    Sends a byte as a PS/2 keyboard does: a start bit that is 0, the 8 bits
    from the lowest one up, a parity bit that makes the number of ones in
    the 9 bits odd, and a stop bit that is 1. A bit is put on the data line
    while the clock line is high, and the chip takes it when the clock line
    falls.
*/
static void kbd_send(uint8_t byte)
{
    uint32_t parity = ~(uint32_t) __builtin_popcount(byte) & 1u;
    uint32_t frame = ((uint32_t) byte << 1) | (parity << 9) | (1u << 10);

    for (uint32_t i = 0; i < KBD_FRAME_BITS; ++i) {
        gpio_put(PIN_KBD_DATA, (frame >> i) & 1u);
        busy_wait_us(KBD_HALF_BIT_US);
        gpio_put(PIN_KBD_CLK, false);
        busy_wait_us(KBD_HALF_BIT_US);
        gpio_put(PIN_KBD_CLK, true);
    }
}

/* Core 0: The Debug Port */

// Reads the bits of the debug port of the chip, the lowest first.
static void debug_read(uint32_t words[DBG_WORDS])
{
    memset(words, 0, DBG_WORDS * sizeof(words[0]));
    for (uint32_t i = 0; i < DBG_BITS; ++i) {
        debug_clock(i == 0);
        if (gpio_get(PIN_DBG_DO)) {
            words[i / 32] |= 1u << (i % 32);
        }
    }
    gpio_put(PIN_DBG_DI, false);
}

// The bits from `high` down to `low` of what the debug port has shown; a field has 32 bits at most.
static uint32_t debug_field(const uint32_t words[DBG_WORDS], uint32_t high, uint32_t low)
{
    uint64_t pair = words[low / 32];
    if (low / 32 + 1u < DBG_WORDS) {
        pair |= (uint64_t) words[low / 32 + 1u] << 32;
    }

    return (uint32_t) ((pair >> (low % 32)) & ((1ull << (high - low + 1u)) - 1u));
}

/*
    Shows what the debug port shows; the comment of `asc_soc.sv` gives the
    place of each field. The bits belong to one moment only while the CPU
    is stopped, and the answer says whether it is.
*/
static void monitor_regs(void)
{
    uint32_t words[DBG_WORDS];
    debug_read(words);

    // Without its wires the output reads as ones or as zeros. An open port always has some bit of cr0
    // or of its state set, and a port that the kernel has locked shows one bit, the one of the lock.
    uint32_t all_and = words[0] & words[1] & words[2] & words[3];
    uint32_t all_or = words[0] | words[1] | words[2] | words[3];
    if (all_and == 0xffffffffu) {
        printf("err regs: the debug port shows only ones; check its three wires\n");
        return;
    }
    if (debug_field(words, DBG_LOCKED, DBG_LOCKED)) {
        printf("ok regs locked=1 stopped=%d\n", is_stopped ? 1 : 0);
        return;
    }
    if (all_or == 0) {
        printf("err regs: the debug port shows only zeros; check its three wires\n");
        return;
    }

    // cr0 has its bits 3 to 0 among the first bits of the port and its bits 5 and 4 at the end
    uint32_t cr0 = debug_field(words, 100, 97) | (debug_field(words, 642, 641) << 4);

    printf("ok regs locked=0 ip=%08lx inst=%08lx cr5=%07lx flags=%02lx cr0=%02lx state=%lu halted=%lu double_fault=%lu "
           "bus_status=%lu bus_state=%04lx at_rest=%lu stopped=%d",
           (unsigned long) debug_field(words, 31, 0), (unsigned long) debug_field(words, 63, 32),
           (unsigned long) debug_field(words, 90, 64), (unsigned long) debug_field(words, 96, 91),
           (unsigned long) cr0, (unsigned long) debug_field(words, 105, 101),
           (unsigned long) debug_field(words, 106, 106), (unsigned long) debug_field(words, 107, 107),
           (unsigned long) debug_field(words, 110, 108), (unsigned long) debug_field(words, 126, 111),
           (unsigned long) debug_field(words, 127, 127), is_stopped ? 1 : 0);

    // The registers follow in words of 32 bits from bit 128 on, in this order
    static const char *const NAMES[] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "fp", "ksp", "lr", "usp", "cr1", "cr2", "cr3", "cr4"
    };
    for (uint32_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); ++i) {
        printf(" %s=%08lx", NAMES[i], (unsigned long) words[4u + i]);
    }
    printf("\n");
}

/* Core 0: The Screen and the Button */

// Sends the display one message: `first` says whether commands or pixels follow.
static bool oled_send(uint8_t first, const uint8_t *bytes, uint32_t len)
{
    uint8_t message[1 + OLED_RAM_WIDTH];

    if (is_oled_serial) {
        // The line D/C says what the bytes are; the display takes each bit at a rise of the clock
        gpio_put(PIN_OLED_DC, first == OLED_DATA);
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t byte = bytes[i];
            for (uint32_t bit = 0x80; bit != 0; bit >>= 1) {
                gpio_put(PIN_OLED_SCL, false);
                gpio_put(PIN_OLED_SDA, (byte & bit) != 0);
                busy_wait_at_least_cycles(OLED_HALF_CYCLES);
                gpio_put(PIN_OLED_SCL, true);
                busy_wait_at_least_cycles(OLED_HALF_CYCLES);
            }
        }
        return true;
    }

    message[0] = first;
    memcpy(message + 1, bytes, len);

    return i2c_write_timeout_us(OLED_I2C, oled_addr, message, len + 1u, false, OLED_TIMEOUT_US) == (int) (len + 1u);
}

/*
    Starts the display. The commands are those of the data sheet of the
    SSD1306 for a display of 128 by 64 pixels with its own charge pump; an
    SH1106 takes them too.

    A display with four pins answers on the two lines at one of two
    addresses. A display with six pins (RST and D/C) may instead take its
    bytes over the same two lines as a clock and a data line, and then it
    cannot answer: if no display answers, the bytes are sent that way.
*/
static void oled_start(void)
{
    static const uint8_t start[] = {
        0xae,       // display off
        0xd5, 0x80, // clock divider
        0xa8, 0x3f, // 64 rows
        0xd3, 0x00, // no offset
        0x40,       // first row 0
        0x8d, 0x14, // charge pump on
        0x20, 0x02, // rows are addressed one at a time
        0xa1, 0xc8, // columns and rows the usual way round
        0xda, 0x12, // layout of the row pins
        0x81, 0xcf, // contrast
        0xd9, 0xf1, // precharge
        0xdb, 0x40, // level of a row that is off
        0xa4, 0xa6, // show the memory, not inverted
        0xaf        // display on
    };

    // A reset; D/C is low, which also chooses the first address
    gpio_put(PIN_OLED_DC, false);
    gpio_put(PIN_OLED_RST, false);
    busy_wait_us_32(OLED_RESET_US);
    gpio_put(PIN_OLED_RST, true);
    busy_wait_us_32(OLED_RESET_US);

    is_oled_serial = false;
    gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_I2C);
    oled_addr = OLED_ADDR;
    if (!oled_send(OLED_COMMAND, start, sizeof(start))) {
        oled_addr = OLED_ADDR_OTHER;
        if (!oled_send(OLED_COMMAND, start, sizeof(start))) {
            is_oled_serial = true;
            gpio_put(PIN_OLED_SCL, true);
            gpio_put(PIN_OLED_SDA, false);
            gpio_set_dir(PIN_OLED_SCL, GPIO_OUT);
            gpio_set_dir(PIN_OLED_SDA, GPIO_OUT);
            gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_SIO);
            gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_SIO);

            /*
                A display that listens this way has counted the clocks of
                the two questions above as bits, 18 of them, and would take
                every byte from here on two bits late: it has no line that
                marks the start of a byte. A second reset makes it count
                from zero.
            */
            gpio_put(PIN_OLED_RST, false);
            busy_wait_us_32(OLED_RESET_US);
            gpio_put(PIN_OLED_RST, true);
            busy_wait_us_32(OLED_RESET_US);
            oled_send(OLED_COMMAND, start, sizeof(start));
        }
    }

    // The memory of the display holds anything after power on; an SH1106 has columns that no frame reaches
    uint8_t dark[OLED_RAM_WIDTH];
    memset(dark, 0, sizeof(dark));
    for (uint32_t page = 0; page < OLED_PAGES; ++page) {
        uint8_t place[] = {(uint8_t) (0xb0u | page), 0x00, 0x10};
        oled_send(OLED_COMMAND, place, sizeof(place));
        oled_send(OLED_DATA, dark, sizeof(dark));
    }
    is_oled_started = true;
}

/*
    Sends a frame to the display. The frame has a bit per pixel in rows; the
    display takes bytes of eight pixels one above the other, a band of eight
    rows at a time. Returns false if a display that answers has stopped
    doing so.
*/
static bool oled_frame(const uint32_t *frame)
{
    if (!is_oled_started) {
        oled_start();
    }

    for (uint32_t page = 0; page < OLED_PAGES; ++page) {
        uint8_t pixels[RP_SCR_WIDTH];
        memset(pixels, 0, sizeof(pixels));
        for (uint32_t j = 0; j < 8; ++j) {
            const uint32_t *row = frame + (page * 8 + j) * (RP_SCR_WIDTH / 32);
            for (uint32_t x = 0; x < RP_SCR_WIDTH; ++x) {
                pixels[x] |= (uint8_t) (((row[x >> 5] >> (x & 31)) & 1u) << j);
            }
        }
        uint8_t place[] = {(uint8_t) (0xb0u | page), (uint8_t) (oled_shift & 0xfu), (uint8_t) (0x10u | (oled_shift >> 4))};
        if (!oled_send(OLED_COMMAND, place, sizeof(place)) || !oled_send(OLED_DATA, pixels, sizeof(pixels))) {
            is_oled_started = false; // the next frame starts the display anew
            return false;
        }
    }
    ++oled_frames;

    return true;
}

// Draws a text into a frame from a column of a row of text on.
static void frame_text(uint32_t *frame, uint32_t row, uint32_t col, const char *text)
{
    for (; *text != '\0' && row < RP_SCR_ROWS; ++text) {
        bridge_draw_char(frame, row << 16 | col << 8 | (uint8_t) *text);
        if (++col == RP_SCR_COLS) {
            col = 0;
            ++row;
        }
    }
}

// Draws the state of the button on the last row of the monitor's frame.
static void monitor_button_line(void)
{
    char text[RP_SCR_COLS + 1];
    snprintf(text, sizeof(text), "button %-4s %-9lu", is_button_down ? "down" : "up", (unsigned long) button_presses);
    frame_text(bridge.scr_sent, RP_SCR_ROWS - 1u, 0, text);
}

/*
    The monitor's own picture, for a check of the display and the button
    with no program: a text, or with no text a line around the whole screen,
    which shows whether all 128 columns are where they belong. The last row
    follows the button until the chip shows a frame of its own. The picture
    is drawn where the chip's frames wait for the display, so the chip must
    be held meanwhile.
*/
static bool monitor_show(const char *text)
{
    memset(bridge.scr_sent, 0, sizeof(bridge.scr_sent));
    if (text[0] != '\0') {
        frame_text(bridge.scr_sent, 0, 0, text);
    } else {
        for (uint32_t y = 0; y < RP_SCR_HEIGHT; ++y) {
            uint32_t *row = bridge.scr_sent + y * (RP_SCR_WIDTH / 32);
            bool is_edge = y == 0 || y == RP_SCR_HEIGHT - 1u;
            for (uint32_t i = 0; i < RP_SCR_WIDTH / 32; ++i) {
                row[i] = is_edge ? 0xffffffffu : 0;
            }
            row[0] |= 1u;
            row[RP_SCR_WIDTH / 32 - 1] |= 0x80000000u;
        }
        char line[RP_SCR_COLS + 1];
        frame_text(bridge.scr_sent, 2, 5, "Almurut II");
        snprintf(line, sizeof(line), "shift %lu", (unsigned long) oled_shift);
        frame_text(bridge.scr_sent, 4, 7, line);
    }
    is_monitor_frame = true;
    if (text[0] != '\0') {
        monitor_button_line();
    }

    return oled_frame(bridge.scr_sent);
}

// Sends the frame that the chip has shown to the display. While the button
// steps the CPU the display is the stepper's, and the frame is dropped.
static void screen_work(void)
{
    if (!bridge_screen_is_busy(&bridge)) {
        return;
    }

    if (!is_stepper_on) {
        is_monitor_frame = false;
        oled_frame(bridge.scr_sent);
    }
    bridge_screen_finish(&bridge);
}

/*
    Shows the console on the display while no program draws: the bridge's
    core 1 draws every printed byte into the frame memory, and this sends
    the frame memory when it has changed, a limited number of times a
    second, so that the terminal is served between the frames. The frame
    memory is sent as it is, with no copy: a byte drawn in the middle of
    the way shows up whole with the next frame.
*/
static void console_screen_work(void)
{
    static uint32_t shown;      // value of `bridge.scr_drawn` at the last frame
    static uint32_t shown_time;

    uint32_t drawn = bridge.scr_drawn;
    uint32_t now = timer_hw->timerawl;
    if (!bridge.scr_console || drawn == shown || is_stepper_on || now - shown_time < CONSOLE_FRAME_US) {
        return;
    }

    shown = drawn;
    is_monitor_frame = false;
    oled_frame(bridge.scr);
    shown_time = timer_hw->timerawl;
}

/*
    The stepper: a debugger of one button. The CPU is stopped; each press
    runs one instruction, and the display shows what the debug port of the
    chip tells: the address of the next instruction, the instruction that
    has run, the flags, and the state. No computer is needed for it.
*/
static void stepper_show(const char *note)
{
    static const char FLAGS[] = "CZSOIU";
    uint32_t words[DBG_WORDS];
    char line[RP_SCR_COLS + 1];

    debug_read(words);
    memset(bridge.scr_sent, 0, sizeof(bridge.scr_sent));

    // A port that the kernel has locked shows no register
    if (debug_field(words, DBG_LOCKED, DBG_LOCKED)) {
        frame_text(bridge.scr_sent, 0, 0, "the debug port");
        frame_text(bridge.scr_sent, 1, 0, "is locked");
        snprintf(line, sizeof(line), "step %lu", (unsigned long) stepper_steps);
        frame_text(bridge.scr_sent, 6, 0, line);
        frame_text(bridge.scr_sent, 7, 0, note);
        is_monitor_frame = false;
        oled_frame(bridge.scr_sent);
        return;
    }

    snprintf(line, sizeof(line), "next %08lx", (unsigned long) debug_field(words, 31, 0));
    frame_text(bridge.scr_sent, 0, 0, line);
    snprintf(line, sizeof(line), "ran  %08lx", (unsigned long) debug_field(words, 63, 32));
    frame_text(bridge.scr_sent, 1, 0, line);

    // A letter for a flag that is set, a dot for one that is clear
    uint32_t flags = debug_field(words, 96, 91);
    char letters[sizeof(FLAGS)];
    for (uint32_t i = 0; i < sizeof(FLAGS) - 1u; ++i) {
        letters[i] = ((flags >> i) & 1u) ? FLAGS[i] : '.';
    }
    letters[sizeof(FLAGS) - 1u] = '\0';
    snprintf(line, sizeof(line), "flags %s", letters);
    frame_text(bridge.scr_sent, 2, 0, line);

    snprintf(line, sizeof(line), "cause %07lx", (unsigned long) debug_field(words, 90, 64));
    frame_text(bridge.scr_sent, 3, 0, line);
    snprintf(line, sizeof(line), "%s%s", debug_field(words, 106, 106) ? "halted " : "",
             debug_field(words, 107, 107) ? "double fault" : "");
    frame_text(bridge.scr_sent, 4, 0, line);
    snprintf(line, sizeof(line), "step %lu", (unsigned long) stepper_steps);
    frame_text(bridge.scr_sent, 6, 0, line);
    frame_text(bridge.scr_sent, 7, 0, note);

    is_monitor_frame = false;
    oled_frame(bridge.scr_sent);
}

// A press of the button while the stepper is on.
static void stepper_press(void)
{
    if (!is_server_running || !is_stopped) {
        stepper_show("the CPU is not held");
        return;
    }

    const char *error = chip_step(true);
    ++stepper_steps;
    stepper_show(error == NULL ? "press: next step" : "waits for interrupt");
}

/*
    Reads the button, which joins its pin to ground while it is down. A new
    level counts when it has stayed for `BUTTON_STABLE_US`, and after it has
    counted the pin is not looked at for `BUTTON_QUIET_US`, so the bounces of
    the contact are not presses and a press is seen within a millisecond.
*/
static void button_work(void)
{
    uint32_t now = timer_hw->timerawl;
    if (is_press_played && (int32_t) (now - press_end) >= 0) {
        is_press_played = false;
    }
    bool is_down = !gpio_get(PIN_BUTTON) || is_press_played;
    if (now - button_counted < BUTTON_QUIET_US && is_button_valid) {
        button_last = is_button_down;
    } else if (is_down != button_last) {
        button_last = is_down;
        button_since = now;
    } else if (is_down != is_button_down && now - button_since > BUTTON_STABLE_US) {
        is_button_down = is_down;
        is_button_valid = true;
        button_counted = now;
        if (is_down) {
            ++button_presses;
        }
        if (is_stepper_on && is_down) {
            stepper_press();
        } else if (is_monitor_frame) {
            monitor_button_line();
            oled_frame(bridge.scr_sent);
        }
    }

    // After a reset of the chip the devices take the level anew; the stepper keeps the button to itself
    if (!is_stepper_on && (bridge.btn_down != 0) != is_button_down) {
        bridge_set_button(&bridge, is_button_down);
    }
}

/* Core 0: The Monitor */

static uint32_t crc32_update(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (uint32_t i = 0; i < 8; ++i) {
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }

    return crc;
}

/*
    Takes a firmware image from the serial port: `size` bytes, which are a
    header, the text section, and the data section, as `asc2-emulated/asc.h`
    defines them. The text section becomes the boot ROM. The data section is
    kept and put into RAM at every reset.
*/
static void monitor_load(uint32_t size)
{
    chip_halt();
    typed_taken = typed_put;
    has_image = false;
    bridge.rom_len = 0;
    image_data_size = 0;

    printf("ok send\n");

    bin_fmt_header_t header = { 0 };
    uint32_t text_end = 0;
    uint32_t data_off = 0;
    uint32_t data_end = 0;
    const char *error = NULL;

    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < size; ++i) {
        int c = getchar_timeout_us(LOAD_TIMEOUT_US);
        if (c == PICO_ERROR_TIMEOUT) {
            printf("err load: only %lu of %lu bytes came\n", (unsigned long) i, (unsigned long) size);
            return;
        }
        crc = crc32_update(crc, (uint8_t) c);

        if (i < sizeof(header)) {
            ((uint8_t *) &header)[i] = (uint8_t) c;
            if (i + 1 == sizeof(header)) {
                // The text section is filled up to a whole word, and the data section follows it in `image`
                text_end = sizeof(header) + header.text_sec_size;
                data_off = (header.text_sec_size + 3u) & ~3u;
                data_end = text_end + header.data_sec_size;
                if (header.magic != BIN_FMT_MAGIC) {
                    error = "the image has an incorrect magic number";
                } else if (header.text_sec_size > BOOT_ROM_SIZE || header.data_sec_size > RAM_SIZE - IMAGE_DATA_ADDR) {
                    error = "a section of the image is too big for the machine";
                } else if (data_off + header.data_sec_size > IMAGE_SIZE) {
                    error = "the image is too big for the memory of the bridge";
                } else if (data_end != size) {
                    error = "the size of the image differs from the sizes in its header";
                } else {
                    memset(image, 0, data_off);
                }
            }
        } else if (error == NULL && i < text_end) {
            image[i - sizeof(header)] = (uint8_t) c;
        } else if (error == NULL && i < data_end) {
            image[data_off + (i - text_end)] = (uint8_t) c;
        }
    }

    if (size < sizeof(header)) {
        error = "the image is shorter than its header";
    }
    if (error != NULL) {
        printf("err load: %s\n", error);
        return;
    }

    bridge.rom_len = data_off;
    image_data_off = data_off;
    image_data_size = header.data_sec_size;
    has_image = true;

    printf("ok load text=%lu data=%lu crc=%08lx\n",
           (unsigned long) header.text_sec_size, (unsigned long) header.data_sec_size, (unsigned long) (crc ^ 0xffffffffu));
}

/*
    The image for the power-up. `save` writes the loaded image into the
    flash, and the bridge that starts finds it there, loads it as `load`
    does, and runs it, so the machine boots with no computer that sends it
    an image. The note is written last, so a save that was cut off leaves
    no image.
*/
typedef struct kept_note
{
    uint32_t magic;     // `KEPT_MAGIC`
    uint32_t rom_len;   // the three numbers that `load` sets
    uint32_t data_off;
    uint32_t data_size;
    uint32_t crc;       // of the bytes of the image
} kept_note_t;

static uint32_t crc32_of(const uint8_t *bytes, uint32_t len)
{
    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < len; ++i) {
        crc = crc32_update(crc, bytes[i]);
    }

    return crc ^ 0xffffffffu;
}

// Returns the note of the image in the flash, or NULL if no whole image is there.
static const kept_note_t *kept_find(void)
{
    const kept_note_t *note = (const kept_note_t *) (XIP_BASE + KEPT_FLASH_OFF);
    const uint8_t *bytes = (const uint8_t *) (XIP_BASE + KEPT_FLASH_OFF + RP_DISK_SEC_SIZE);

    if (note->magic != KEPT_MAGIC || note->data_off > IMAGE_SIZE || note->data_size > IMAGE_SIZE - note->data_off ||
        note->rom_len != note->data_off || note->data_size > RAM_SIZE - IMAGE_DATA_ADDR ||
        crc32_of(bytes, note->data_off + note->data_size) != note->crc) {
        return NULL;
    }

    return note;
}

static void kept_forget(void)
{
    flash_begin();
    uint32_t irqs = save_and_disable_interrupts();
    flash_range_erase(KEPT_FLASH_OFF, RP_DISK_SEC_SIZE);
    restore_interrupts(irqs);
    flash_end();
}

static void kept_save(void)
{
    uint32_t len = image_data_off + image_data_size;

    kept_forget();
    for (uint32_t off = 0; off < len; off += RP_DISK_SEC_SIZE) {
        flash_write_sector(KEPT_FLASH_OFF + RP_DISK_SEC_SIZE + off, image + off);
    }

    // `kept_forget` has left its sector for the note
    kept_note_t note = { KEPT_MAGIC, bridge.rom_len, image_data_off, image_data_size, crc32_of(image, len) };
    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xff, sizeof(page));
    memcpy(page, &note, sizeof(note));
    flash_begin();
    uint32_t irqs = save_and_disable_interrupts();
    flash_range_program(KEPT_FLASH_OFF, page, sizeof(page));
    restore_interrupts(irqs);
    flash_end();
}

// Loads the image of the flash, if there is one, and runs it.
static void kept_boot(void)
{
    const kept_note_t *note = kept_find();
    if (note == NULL) {
        return;
    }

    memcpy(image, (const uint8_t *) (XIP_BASE + KEPT_FLASH_OFF + RP_DISK_SEC_SIZE), note->data_off + note->data_size);
    bridge.rom_len = note->rom_len;
    image_data_off = note->data_off;
    image_data_size = note->data_size;
    has_image = true;
    chip_run(false);
}

static uint32_t peek_word(uint32_t addr)
{
    uint32_t word = 0;
    if (addr < RAM_SIZE) {
        memcpy(&word, ram + addr, sizeof(word));
    } else if (addr >= BOOT_ROM_BASE && addr - BOOT_ROM_BASE < bridge.rom_len) {
        memcpy(&word, image + (addr - BOOT_ROM_BASE), sizeof(word));
    } else if (addr >= HIGH_BASE && addr - HIGH_BASE < HIGH_SIZE) {
        memcpy(&word, high + (addr - HIGH_BASE), sizeof(word));
    }

    return word;
}

static void monitor_stat(void)
{
    printf("ok stat running=%d clock_hz=%lu requests=%lu repeats=%lu recoveries=%lu quiet=%lu late=%lu lost=%lu guard=%lu "
           "ack_repairs=%lu console_lost=%lu typed_lost=%lu disk_reads=%lu disk_writes=%lu disk_errors=%lu watchdog=%d "
           "watchdog_resets=%lu disk_flash=0x%08lx disk_size=%lu stopped=%d fetches=%lu\n",
           is_server_running ? 1 : 0, (unsigned long) chip_clock_hz(),
           (unsigned long) bus_stats.requests, (unsigned long) bus_stats.repeats, (unsigned long) bus_stats.recoveries,
           (unsigned long) bus_stats.quiet, (unsigned long) bus_stats.late, (unsigned long) bus_stats.lost,
           (unsigned long) bus_stats.guard, (unsigned long) bus_stats.ack_repairs, (unsigned long) bridge.con_out_lost,
           (unsigned long) typed_lost, (unsigned long) disk_reads, (unsigned long) disk_writes, (unsigned long) disk_errors,
           is_watchdog_on ? 1 : 0, (unsigned long) watchdog_resets, (unsigned long) (XIP_BASE + DISK_FLASH_OFF),
           (unsigned long) (DISK_SECS * RP_DISK_SEC_SIZE), is_stopped ? 1 : 0, (unsigned long) fetch_cnt);
}

static void monitor_trace(void)
{
    uint32_t cnt = trace_cnt;
    for (uint32_t i = (cnt > TRACE_SIZE) ? cnt - TRACE_SIZE : 0; i < cnt; ++i) {
        trace_entry_t *entry = &trace[i & (TRACE_SIZE - 1u)];
        printf("%lu: header %02lx address %08lx data %08lx answer %lu\n",
               (unsigned long) i, (unsigned long) entry->hdr, (unsigned long) entry->addr,
               (unsigned long) entry->data, (unsigned long) entry->stat);
    }
    printf("ok trace %lu\n", (unsigned long) cnt);
}

static void monitor_faults(void)
{
    static const char *const kinds[] = {"quiet in the request", "late", "quiet in the answer", "guard", "guard without a request"};

    uint32_t cnt = fault_cnt;
    for (uint32_t i = (cnt > FAULT_LOG_SIZE) ? cnt - FAULT_LOG_SIZE : 0; i < cnt; ++i) {
        fault_entry_t *entry = &faults[i & (FAULT_LOG_SIZE - 1u)];
        printf("%lu: after %lu requests: %s, header %02lx address %08lx data %08lx, %lu of %lu, pins %08lx",
               (unsigned long) i, (unsigned long) entry->request, kinds[entry->kind], (unsigned long) entry->hdr,
               (unsigned long) entry->addr, (unsigned long) entry->wdata, (unsigned long) entry->done,
               (unsigned long) entry->wanted, (unsigned long) entry->pins);
        if (entry->has_next) {
            printf("; then header %02lx address %08lx data %08lx", (unsigned long) entry->next_hdr,
                   (unsigned long) entry->next_addr, (unsigned long) entry->next_wdata);
        }
        printf("\n");
    }
    printf("ok faults %lu\n", (unsigned long) cnt);
}

static void monitor_command(char *line)
{
    char *args = strchr(line, ' ');
    if (args != NULL) {
        *args++ = '\0';
    } else {
        args = line + strlen(line);
    }

    char *end = NULL;
    if (strcmp(line, "load") == 0) {
        unsigned long size = strtoul(args, &end, 0);
        if (end == args || *end != '\0') {
            printf("err load: the size of the image is missing\n");
        } else {
            monitor_load((uint32_t) size);
        }
    } else if (strcmp(line, "run") == 0) {
        bool is_held = strcmp(args, "stopped") == 0;
        if (!has_image) {
            printf("err run: no image is loaded\n");
        } else if (args[0] != '\0' && !is_held) {
            printf("err run: the only word that may follow is `stopped`\n");
        } else {
            // What was typed before is not for this run
            typed_taken = typed_put;
            printf("ok run\n");
            chip_run(is_held);
        }
    } else if (strcmp(line, "save") == 0) {
        if (!has_image) {
            printf("err save: no image is loaded\n");
        } else {
            kept_save();
            printf(kept_find() != NULL ? "ok save\n" : "err save: the flash does not hold the image\n");
        }
    } else if (strcmp(line, "forget") == 0) {
        kept_forget();
        printf("ok forget\n");
    } else if (strcmp(line, "halt") == 0) {
        chip_halt();
        typed_taken = typed_put;
        printf("ok halt\n");
    } else if (strcmp(line, "clock") == 0) {
        unsigned long div = strtoul(args, &end, 0);
        if (end == args || *end != '\0' || div < 2 || div > 65535) {
            printf("err clock: the divider is a number from 2 to 65535\n");
        } else if (is_server_running) {
            printf("err clock: the chip runs; halt it first\n");
        } else {
            clock_start((uint32_t) div);
            printf("ok clock %lu Hz\n", (unsigned long) chip_clock_hz());
        }
    } else if (strcmp(line, "peek") == 0) {
        unsigned long addr = strtoul(args, &end, 0);
        unsigned long cnt = (end != args && *end == ' ') ? strtoul(end + 1, NULL, 0) : 1;
        if (end == args || addr % 4 != 0 || cnt == 0 || cnt > 256) {
            printf("err peek: the address is a multiple of 4, and the number of words is at most 256\n");
        } else {
            for (unsigned long i = 0; i < cnt; ++i) {
                printf("%08lx: %08lx\n", addr + (4 * i), (unsigned long) peek_word((uint32_t) (addr + (4 * i))));
            }
            printf("ok peek\n");
        }
    } else if (strcmp(line, "poke") == 0) {
        unsigned long addr = strtoul(args, &end, 0);
        char *value_end = NULL;
        unsigned long value = (end != args && *end == ' ') ? strtoul(end + 1, &value_end, 0) : 0;
        if (end == args || value_end == NULL || value_end == end + 1 || addr % 4 != 0 || addr >= RAM_SIZE) {
            printf("err poke: the address is a multiple of 4 in RAM, and a value follows it\n");
        } else if (is_server_running) {
            printf("err poke: the chip runs; halt it first\n");
        } else {
            uint32_t word = (uint32_t) value;
            memcpy(ram + addr, &word, sizeof(word));
            printf("ok poke\n");
        }
    } else if (strcmp(line, "stat") == 0) {
        monitor_stat();
    } else if (strcmp(line, "trace") == 0) {
        monitor_trace();
    } else if (strcmp(line, "faults") == 0) {
        monitor_faults();
    } else if (strcmp(line, "stop") == 0) {
        if (!is_server_running) {
            printf("err stop: the chip is held in reset; `run stopped` starts it with its CPU held\n");
        } else {
            gpio_put(PIN_STOP, true);
            is_stopped = true;
            if (chip_wait_still()) {
                printf("ok stop\n");
            } else {
                printf("err stop: the chip goes on with STOP high\n");
            }
        }
    } else if (strcmp(line, "step") == 0) {
        unsigned long cnt = (args[0] != '\0') ? strtoul(args, &end, 0) : 1;
        if ((args[0] != '\0' && (end == args || *end != '\0')) || cnt == 0 || cnt > MAX_STEPS) {
            printf("err step: the number of instructions is from 1 to %d\n", MAX_STEPS);
        } else if (!is_server_running || !is_stopped) {
            printf("err step: the CPU is not stopped\n");
        } else {
            const char *error = NULL;
            for (unsigned long i = 0; i < cnt && error == NULL; ++i) {
                error = chip_step(false);
            }
            if (error == NULL) {
                printf("ok step %lu\n", cnt);
            } else {
                printf("err step: %s\n", error);
            }
        }
    } else if (strcmp(line, "go") == 0) {
        gpio_put(PIN_STOP, false);
        is_stopped = false;
        is_stepper_on = false;
        printf("ok go\n");
    } else if (strcmp(line, "stepper") == 0) {
        if (strcmp(args, "off") == 0) {
            is_stepper_on = false;
            printf("ok stepper off; the CPU stays stopped until `go`\n");
        } else if (strcmp(args, "on") != 0) {
            printf("err stepper: on or off\n");
        } else if (!is_server_running) {
            printf("err stepper: the chip is held in reset; `run` or `run stopped` starts it\n");
        } else {
            gpio_put(PIN_STOP, true);
            is_stopped = true;
            if (chip_wait_still()) {
                is_stepper_on = true;
                stepper_steps = 0;
                stepper_show("press: next step");
                printf("ok stepper on; each press of the button runs one instruction\n");
            } else {
                printf("err stepper: the chip goes on with STOP high\n");
            }
        }
    } else if (strcmp(line, "regs") == 0) {
        if (!is_server_running) {
            printf("err regs: the chip is held in reset\n");
        } else {
            monitor_regs();
        }
    } else if (strcmp(line, "oled") == 0) {
        if (is_server_running) {
            printf("err oled: the chip runs and the display is its own; halt it first\n");
        } else if (monitor_show(args)) {
            if (is_oled_serial) {
                printf("ok oled sent over the clock and the data line; no display answered at 0x%02x or 0x%02x, and a display cannot answer this way, so look at it\n", OLED_ADDR, OLED_ADDR_OTHER);
            } else {
                printf("ok oled at 0x%02x\n", oled_addr);
            }
        } else {
            printf("err oled: the display stopped answering; check its wires\n");
        }
    } else if (strcmp(line, "shift") == 0) {
        unsigned long value = strtoul(args, &end, 0);
        if (end == args || *end != '\0' || value > OLED_RAM_WIDTH - RP_SCR_WIDTH) {
            printf("err shift: a number from 0 to %d\n", OLED_RAM_WIDTH - RP_SCR_WIDTH);
        } else {
            oled_shift = (uint32_t) value;
            is_oled_started = false; // the next frame clears the display and is sent whole
            printf("ok shift %lu\n", (unsigned long) oled_shift);
        }
    } else if (strcmp(line, "press") == 0) {
        unsigned long ms = (args[0] != '\0') ? strtoul(args, &end, 0) : 100;
        if ((args[0] != '\0' && (end == args || *end != '\0')) || ms == 0 || ms > 60000) {
            printf("err press: the time is a number of milliseconds from 1 to 60000\n");
        } else {
            press_end = timer_hw->timerawl + (uint32_t) ms * 1000u;
            is_press_played = true;
            printf("ok press %lu ms\n", ms);
        }
    } else if (strcmp(line, "frame") == 0) {
        if (bridge_screen_is_busy(&bridge)) {
            printf("err frame: a frame is on its way to the display; ask again\n");
        } else {
            // The console's text is in the frame memory itself; a program's frame is the copy that it has shown
            const uint32_t *frame = bridge.scr_console ? bridge.scr : bridge.scr_sent;
            for (uint32_t y = 0; y < RP_SCR_HEIGHT; ++y) {
                char row[RP_SCR_WIDTH + 1];
                for (uint32_t x = 0; x < RP_SCR_WIDTH; ++x) {
                    row[x] = ((frame[y * (RP_SCR_WIDTH / 32) + (x >> 5)] >> (x & 31)) & 1u) ? '#' : '.';
                }
                row[RP_SCR_WIDTH] = '\0';
                printf("%s\n", row);
            }
            printf("ok frame frames=%lu%s\n", (unsigned long) bridge.scr_done, bridge.scr_console ? " console" : "");
        }
    } else if (strcmp(line, "button") == 0) {
        printf("ok button down=%d presses=%lu\n", is_button_down ? 1 : 0, (unsigned long) button_presses);
    } else if (strcmp(line, "key") == 0) {
        uint8_t keys[MAX_KEYS];
        uint32_t cnt = 0;
        bool is_valid = true;
        for (char *pos = args; *pos != '\0' && is_valid;) {
            unsigned long key = strtoul(pos, &end, 16);
            is_valid = end != pos && key <= 0xff && (*end == ' ' || *end == '\0') && cnt < MAX_KEYS;
            if (is_valid) {
                keys[cnt++] = (uint8_t) key;
                pos = end + ((*end == ' ') ? 1 : 0);
            }
        }
        if (!is_valid || cnt == 0) {
            printf("err key: bytes in hex follow, %d at most\n", MAX_KEYS);
        } else {
            for (uint32_t i = 0; i < cnt; ++i) {
                kbd_send(keys[i]);
                busy_wait_us(KBD_GAP_US);
            }
            printf("ok key %lu\n", (unsigned long) cnt);
        }
    } else if (strcmp(line, "watchdog") == 0) {
        if (strcmp(args, "0") != 0 && strcmp(args, "1") != 0) {
            printf("err watchdog: 0 turns it off, 1 turns it on\n");
        } else {
            is_watchdog_on = args[0] == '1';
            printf("ok watchdog %d\n", is_watchdog_on ? 1 : 0);
        }
    } else if (line[0] != '\0') {
        printf("err %s: no such command\n", line);
    }
}

/* Core 0: The Console */

/*
    Moves bytes between the serial port and the console of the chip. Typed
    bytes wait in `typed` until the console's queue has room, so a program
    gets the lines that were typed while it did not read. While `typed` is
    full, nothing is read from the serial port, so a long text that is
    pasted into the terminal arrives whole. If it stays full for half a
    second, the program does not take its input, and the bytes that come
    are dropped: the key of the monitor may be among them, and the monitor
    is there for a program that has stopped. The Enter key of a terminal
    sends a carriage return, which the chip gets as a line feed.
*/
static void console_work(void)
{
    static bool is_in_monitor;
    static char line[MAX_LINE_LEN];
    static size_t line_len;
    static bool was_full;      // `typed` has been full since `full_start`
    static uint32_t full_start;
    static bool was_return;

    uint8_t byte;
    while (bridge_take_con_output(&bridge, &byte)) {
        putchar(byte);
    }

    while (typed_taken != typed_put && bridge_put_con_input(&bridge, typed[typed_taken % TYPED_SIZE])) {
        ++typed_taken;
    }

    for (uint32_t i = 0; i < CON_IN_SIZE; ++i) {
        bool is_full = typed_put - typed_taken == TYPED_SIZE;
        if (!is_full) {
            was_full = false;
        } else if (!was_full) {
            was_full = true;
            full_start = timer_hw->timerawl;
            return;
        } else if (timer_hw->timerawl - full_start < TYPED_WAIT_US) {
            return;
        }

        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            return;
        }

        if (is_in_monitor) {
            if (c == '\r' || c == '\n') {
                line[line_len] = '\0';
                line_len = 0;
                is_in_monitor = false;
                printf("\n");
                monitor_command(line);
                return;
            } else if (c == '\b' || c == 0x7f) {
                if (line_len > 0) {
                    --line_len;
                    printf("\b \b");
                }
            } else if (line_len < MAX_LINE_LEN - 1) {
                line[line_len++] = (char) c;
                putchar(c);
            }
        } else if (c == MONITOR_KEY) {
            is_in_monitor = true;
            line_len = 0;
            printf("\n" PROGRAM_NAME "> ");
        } else if (c == '\n' && was_return) {
            // A terminal that sends both for the Enter key has sent the line feed already
        } else if (is_full) {
            ++typed_lost;
        } else {
            typed[typed_put % TYPED_SIZE] = (uint8_t) ((c == '\r') ? '\n' : c);
            ++typed_put;
        }
        was_return = c == '\r';
    }
}

int main(void)
{
#if PICO_RP2040
    // The RP2040 needs a higher core voltage for this system clock
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(10);
#endif
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    bridge.ram = ram;
    bridge.rom = image;
    bridge.high = high;
    bridge.rom_len = 0;
    bridge_reset(&bridge);

    pins_init();
#if TT_DEMOBOARD_V3
    pio_set_gpio_base(pio0, PIO_GPIO_BASE);
#endif
    pio_offset = pio_add_program(pio0, &rpbus_program);
    clock_start(CHIP_CLOCK_DIV);
    chip_halt();
#if TT_DEMOBOARD_V3
    design_select();
#endif

    stdio_usb_init();
    kept_boot();

    for (;;) {
        console_work();
        disk_work();
        watchdog_work();
        button_reset_work();
        button_work();
        screen_work();
        console_screen_work();
    }
}
