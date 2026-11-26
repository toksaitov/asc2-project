/*
 * Bench Test 4, Pico side: the RP end of bus protocol v1
 * (asc2-kb/bus-protocol-v1.md) serving the PC-style memory map to the chip
 * side running in the DE10: main memory in SRAM, a small ROM (the BIOS,
 * served from a RAM copy), a disk in flash sectors, and device registers
 * (timer, console, interrupt, scratch). Nothing executes in place from
 * flash: the chip loads from the disk into RAM and runs from RAM.
 *
 * As on the Tiny Tapeout devkit's RP2350, the system clock is 150 MHz
 * and the byte handshake runs in PIO (rpbus.pio). Core 1 decodes and
 * answers transactions from RAM-resident code and does nothing else.
 * Core 0 handles USB commands from the host, raises interrupts at random
 * times and does the slow flash work for the disk (erase and program).
 * The bus keeps serving every address while the flash is busy: nothing
 * the chip can read lives in flash, so a transaction never waits for it.
 *
 * Protocol v1 on this side:
 * * the status byte carries the header's sequence bit: {SEQ, ~SEQ, 000,
 *   code}, so the chip can tell a fresh answer from a stale or broken one;
 * * a repeat of the last transaction (same header with the same SEQ,
 *   same address and write data) is answered from a copy of the last
 *   answer and not done again, so a retried write or console read
 *   happens once; the copy is dropped at every server start;
 * * when the chip goes quiet mid-transaction for t_rp_us (the RP
 *   timeout, 4096 chip clocks, far below the chip's 65535), core 1
 *   releases uio, waits until REQ has been still for t_rp_us, puts ACK
 *   back level with REQ if the last REQ change went unanswered (in the
 *   middle of the chip's quiet time), and restarts the handshake waiting
 *   for a header;
 * * before driving an answer, core 1 checks the chip has not waited
 *   longer than t_rp_us; if it has, the chip may be about to retry, so
 *   core 1 recovers as above instead of driving;
 * * the PIO ignores REQ pulses shorter than 3 cycles (rpbus.pio).
 *
 * Pins (the Test 2 directions):
 *   GP0  RST_N   out  held low until the host releases it
 *   GP1  CLK     out  PWM, sys_clk / divider
 *   GP2-9 uio    PIO, driven only during the RP phase of a transaction
 *   GP10 REQ     in   (PIO input)
 *   GP11 ACK     out  PIO
 *   GP12 IRQ     out  level, high while an interrupt is pending
 *   GP13 DBG_CK  in   unused here
 *   GP14 DBG_DI  out  PIO side-set: high while the Pico drives uio
 *   GP15 DBG_DO  in   high while the chip drives uio
 *
 * Safety net (bench only): the PIO program checks DBG_DO before turning
 * its uio outputs on. If the chip claims uio, the PIO stops with its
 * outputs off and core 1 counts a violation and halts (or, after "V 1",
 * recovers). The DE10 runs the same check against DBG_DI.
 *
 * Memory map served:
 *   0x000000-0x02FFFF  RAM, 192 KB; 0x02F000 and up is kernel-only
 *   0x800000-0x803FFF  ROM, 16 KB, served from a RAM copy (word at offset
 *                      o holds h(o, seed) after "f seed"); read and
 *                      fetch, writes get BUS_ERROR
 *   0xFF0000  ID        read-only, "ALM2"
 *   0xFF0004  SCRATCH   read-write
 *   0xFF0008  TXCOUNT   read-only, transactions served
 *   0xFF000C  TIMER     read-only, microseconds
 *   0xFF0010  CON_STAT  bit 0 an input byte is waiting, bit 1 output ready
 *   0xFF0014  CON_DATA  read: the next input byte, or 0x100 if none;
 *                       write: output byte (the bench checks it against
 *                       the input stream: the chip echoes)
 *   0xFF0018  IRQ_ACK   read: the IRQ line; write: drop it
 *   0xFF0020  DISK_SEC  sector, 0-63
 *   0xFF0024  DISK_BUF  RAM address of a 4 KB buffer, word aligned
 *   0xFF0028  DISK_CMD  write 1: read the sector into the buffer;
 *                       write 2: write the buffer to the sector
 *   0xFF002C  DISK_STAT bit 0 busy, bit 1 error (sticky), [31:16] commands
 *                       done
 * Device registers take 4-byte data accesses only. The buffer must not
 * be touched while the disk is busy (the RP reads or writes it then).
 *
 * Flash: the firmware from offset 0, the disk at 0x1C0000-0x1FFFFF
 * (64 sectors of 4 KB).
 *
 * The console input is a fixed stream, byte j = h(j, 0xC0DE0000) & 0xFF,
 * that arrives at one byte per 8 us (a 1.25 Mbaud UART) into a 64-byte
 * FIFO that never drops: the stream waits while the FIFO is full.
 *
 * Commands (one line each, one answer line starting "ok" or "err"):
 *   i      identify
 *   g      configure the pins and (re)start the server; RST_N goes low
 *   c N    CLK = sys_clk / N (N >= 2); "c 0" stops the clock; only while
 *          RST_N is low, so the chip never sees a clock glitch
 *   r 0    stop the server, release uio, hold the chip in reset
 *   r 1    release the chip; needs "g" first, and CLK on
 *   s      statistics
 *   z      clear RAM, scratch and disk state (only while RST_N is low)
 *   d M S  drive strength M mA (2, 4, 8, 12) and slew S (0 slow, 1 fast)
 *          for ACK, IRQ, DBG_DI and uio (default 4 0)
 *   k M S  the same for CLK (default 4 0: gentle, standing in for the
 *          devkit's 47 ohm series resistor)
 *   f SEED fill the ROM copy with h(o, SEED) (RST_N low)
 *   v SEED check the ROM copy against h(o, SEED)
 *   T US   RP timeout in microseconds, 10 to 100000 (default 100); the
 *          host sets it to 4096 chip clocks; the ACK repair time is 24
 *          times that (98304 chip clocks, see rp_resync)
 *   F M N  inject a fault into every Nth transaction (N 0: off): M 1
 *          drop it before doing it, 2 answer with the wrong SEQ, 3 stall
 *          after the header, 4 do it and lose the answer, 5 do it and
 *          answer too late (the stale check must catch it)
 *   V 0|1  on a safety-net violation halt (0, the default) or recover as
 *          the devkit firmware would (1); every violation is counted
 *   n 0|1  random interrupts every 100-1000 us off or on
 *   t      dump the last TRACE_N transactions served
 *   w      dump the last REQ glitches seen (time in us, 0 high, 1 low)
 *   S KHZ  system clock in kHz, 50000 to 200000 (only after "r 0"); for
 *          measuring how the transaction rate scales with the RP clock
 *   p 0|1  core 1 cycle profile off or on (cleared when turned on)
 *   P      print the profile: cycles per transaction waiting for a
 *          header, receiving, deciding, answering
 *   x      stop the server; every pin back to an input
 *
 * Bench diagnostics: PIO block 1 watches REQ and counts pulses shorter
 * than about 20 ns ("req_gl_hi", "req_gl_lo" in the statistics). A real
 * REQ toggle holds for a whole round trip; a short pulse is noise on the
 * wire, and the handshake in PIO block 0 would take it as an event.
 * Interrupts are raised only while RST_N is high.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/timer.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "rpbus.pio.h"

#define PIN_RST     0u
#define PIN_CLK     1u
#define UIO_SHIFT   2u
#define PIN_REQ     10u
#define PIN_ACK     11u
#define PIN_IRQ     12u
#define PIN_DBG_CK  13u
#define PIN_PDRV    14u
#define PIN_CDRV    15u
#define NUM_PINS    16u

#define UIO_MASK    (0xFFu << UIO_SHIFT)
#define ACK_BIT     (1u << PIN_ACK)
#define IRQ_BIT     (1u << PIN_IRQ)
#define PDRV_BIT    (1u << PIN_PDRV)
#define RST_BIT     (1u << PIN_RST)

#define SYS_KHZ     150000u

#define RAM_SIZE    0x30000u
#define KERNEL_BASE 0x2F000u
#define BIOS_BASE    0x800000u
#define BIOS_SIZE    0x4000u
#define DEV_BASE    0xFF0000u
#define DEV_SIZE    0x30u
#define DEV_ID      0x324D4C41u

#define DISK_OFF    0x1C0000u
#define DISK_SECS   64u
#define SEC_SIZE    4096u

#define ST_OK         0x00u
#define ST_BUS_ERROR  0x01u
#define ST_PROTECTION 0x02u
#define ST_MISALIGNED 0x03u
#define ST_BAD_NEXT   0x04u

#define SM            0u
#define FS_RX_EMPTY   (1u << (PIO_FSTAT_RXEMPTY_LSB + SM))
#define FS_TX_FULL    (1u << (PIO_FSTAT_TXFULL_LSB + SM))
#define CMD_RECV      0u
#define CMD_DRIVE(b)  ((uint32_t)(b) | 0x100u | (0xFFu << 9))
#define SM_EN         (1u << (PIO_CTRL_SM_ENABLE_LSB + SM))
#define SM_RESTART    (1u << (PIO_CTRL_SM_RESTART_LSB + SM))

#define CON_FIFO      64u

#define PH_STOPPED  0u
#define PH_HDR      1u
#define PH_RECV     2u
#define PH_ANSWER   3u
#define PH_VIOL     4u

#define TRACE_N     512u

static uint8_t ram[RAM_SIZE] __attribute__((aligned(4)));
static uint8_t rom[BIOS_SIZE] __attribute__((aligned(4)));

static uint pio_offset;

/* Shared between the cores. */
static volatile uint32_t flash_req;
static volatile uint32_t flash_ack;
/* Instructions core 1 runs in the state machine to recover (v1),
 * encoded on core 0 so core 1 calls nothing in flash. */
static uint32_t ins_osr0;
static uint32_t ins_rel;
static uint32_t ins_rise;
static uint32_t ins_fall;
static uint32_t ins_ack0;
static uint32_t ins_ack1;
static volatile uint32_t t_rp_us = 100u;
static volatile uint32_t t_fix_us = 2400u;
static volatile uint32_t inj_mode;
static volatile uint32_t inj_every;
/* 'V 1': on a safety-net violation, recover (as on the devkit) instead of
 * halting. Every violation is still counted. */
static volatile uint32_t viol_recover;
static volatile bool server_running;
static volatile uint32_t core1_phase;

static volatile uint32_t dev_scratch;
static volatile uint32_t con_t;
static volatile uint32_t con_avail;
static volatile uint32_t con_deliv;
static volatile uint32_t con_echo;
static volatile uint32_t con_bad;
static volatile uint32_t irq_t;
static volatile uint32_t disk_sec;
static volatile uint32_t disk_buf;
static volatile uint32_t disk_cmd;
static volatile uint32_t disk_busy;
static volatile uint32_t disk_err;
static volatile uint32_t disk_done;
static volatile uint32_t disk_go_cmd;
static volatile uint32_t disk_go_sec;
static volatile uint32_t disk_go_buf;

static volatile uint32_t stat_tx;
static volatile uint32_t stat_faults;
static volatile uint32_t stat_viol;
static volatile uint32_t stat_ram_rd;
static volatile uint32_t stat_ram_wr;
static volatile uint32_t stat_fetch;
static volatile uint32_t stat_rom;
static volatile uint32_t stat_dev;
static volatile uint32_t stat_replay;
static volatile uint32_t stat_resync;
static volatile uint32_t stat_rp_to;
static volatile uint32_t stat_stale;
static volatile uint32_t stat_inj;
static volatile uint32_t stat_ackfix;
static volatile uint32_t stat_irq;
static volatile uint32_t stat_irq_raised;
static volatile uint32_t stat_irq_max;
static volatile uint32_t stat_irq_sum;
static volatile uint32_t stat_disk_w;
static volatile uint32_t stat_disk_r;
static volatile uint32_t stat_disk_bad;
static volatile uint32_t stat_disk_wmax;
static volatile uint32_t stat_disk_rmax;

/* Core 1 profile, in system clock cycles (SysTick of core 1). */
static volatile uint32_t prof_on;
static uint64_t prof_idle;
static uint64_t prof_recv;
static uint64_t prof_cmp;
static uint64_t prof_resp;
static uint32_t prof_n;

/* REQ glitches (core 0, from PIO block 1). */
#define GL_N 64u
static uint32_t gl_t[GL_N];
static uint32_t gl_k[GL_N];
static uint32_t gl_n;
static uint32_t req_gl_hi;
static uint32_t req_gl_lo;
static uint watch_offset;

static uint32_t trace_a[TRACE_N];
static uint32_t trace_s[TRACE_N];
static uint32_t trace_d[TRACE_N];
static uint32_t trace_t[TRACE_N];

/* Core 0 only. */
static uint32_t clk_div;
static bool irq_gen;
static uint32_t irq_next;
static uint32_t rnd = 0x2545F491u;
static enum gpio_drive_strength out_drive = GPIO_DRIVE_STRENGTH_4MA;
static enum gpio_slew_rate out_slew = GPIO_SLEW_RATE_SLOW;
static enum gpio_drive_strength clk_drive = GPIO_DRIVE_STRENGTH_4MA;
static enum gpio_slew_rate clk_slew = GPIO_SLEW_RATE_SLOW;

/* The expected-data function shared with the DE10 sequencer. */
static __force_inline uint32_t h(uint32_t a, uint32_t s)
{
    uint32_t x = (a & 0xFFFFFFu) ^ s;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/*
 * Core 1. Everything below up to the core 0 section runs from RAM and
 * calls nothing in flash (no switch tables, no division, no library
 * calls), because core 0 turns the flash off to erase and program it.
 * Core 1 confirms flash_req at safe points, where it is not reading the
 * flash, by copying it into flash_ack.
 */

static __force_inline void flash_sync(void)
{
    uint32_t r = flash_req;

    if (r != flash_ack) {
        __dmb();
        flash_ack = r;
    }
}


/*
 * v1 recovery. Stop the state machine and release uio at once (ACK keeps
 * its level), wait until REQ has been still for t_rp_us, then restart the
 * handshake at REQ's present level with a receive queued, so the next REQ
 * change is taken as a header.
 *
 * The PIO answers an event by setting ACK to REQ's new level, so between
 * transactions ACK equals REQ. If the RP gave up without answering the
 * chip's last REQ change, ACK differs from REQ, and the first answer after
 * the restart would set ACK to the level it already has: the chip would
 * see nothing. So in that case ACK is set equal to REQ first, but only in
 * the middle of the chip's quiet time: the chip waits 65535 clocks for the
 * missing answer, then stays quiet 65535 clocks, taking ACK's level as its
 * new baseline and nothing more. t_fix_us (98304 chip clocks after the
 * last REQ change, 24 x T_rp) is that middle, with 32768 clocks of margin
 * each way. A REQ change while waiting is another unanswered attempt and
 * starts the wait again.
 */
#define REC_N 32u
static uint32_t rec_t[REC_N];
static uint32_t rec_w[REC_N];
static uint32_t rec_i[REC_N];
static uint32_t rec_p[REC_N];
static volatile uint32_t rec_n;

static void __not_in_flash_func(rp_resync)(uint32_t why, uint32_t info)
{
    uint32_t pc0 = pio0_hw->sm[SM].addr;
    uint32_t fs0 = pio0_hw->flevel;
    uint32_t req0 = (sio_hw->gpio_in >> PIN_REQ) & 1u;
    uint32_t changes = 0;
    hw_clear_bits(&pio0_hw->ctrl, SM_EN);
    pio0_hw->sm[SM].instr = ins_osr0;
    pio0_hw->sm[SM].instr = ins_rel;      /* uio inputs, PDRV low */
    uint32_t lvl = (sio_hw->gpio_in >> PIN_REQ) & 1u;
    uint32_t t0 = timer_hw->timerawl;
    uint32_t fixed = 0;
    for (;;) {
        uint32_t l = (sio_hw->gpio_in >> PIN_REQ) & 1u;
        uint32_t now = timer_hw->timerawl;
        if (l != lvl) {
            lvl = l;
            t0 = now;
            changes++;
        }
        uint32_t ack = (sio_hw->gpio_in >> PIN_ACK) & 1u;
        if (ack == lvl) {
            if (now - t0 >= t_rp_us) {
                break;
            }
        } else if (now - t0 >= t_fix_us) {
            pio0_hw->sm[SM].instr = lvl ? ins_ack1 : ins_ack0;
            stat_ackfix++;
            fixed = 1;
            break;
        }
        flash_sync();
    }
    hw_set_bits(&pio0_hw->ctrl, SM_RESTART);
    hw_xor_bits(&pio0_hw->sm[SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);
    hw_xor_bits(&pio0_hw->sm[SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);
    pio0_hw->sm[SM].instr = lvl ? ins_fall : ins_rise;
    hw_set_bits(&pio0_hw->ctrl, SM_EN);
    pio0_hw->txf[SM] = CMD_RECV;
    stat_resync++;
    uint32_t k = rec_n & (REC_N - 1u);
    rec_t[k] = timer_hw->timerawl;
    rec_w[k] = why | (req0 << 4) | (lvl << 5) | (fixed << 6) | ((changes > 15u ? 15u : changes) << 8)
               | ((pc0 - ins_rise) << 16) | (fs0 << 24);
    rec_i[k] = info;
    rec_p[k] = stat_tx;
    rec_n++;
}

/*
 * Safety net tripped: the PIO found the chip driving uio when it was about
 * to drive, and stopped with its outputs off. The bench default halts so
 * that a violation is loud. With viol_recover set it recovers like any
 * other fault the RP leaves unanswered: the chip times out and retries,
 * and the retry gets the copied answer.
 */
static void __not_in_flash_func(on_viol)(void)
{
    stat_viol++;
    if (viol_recover) {
        pio0_hw->irq = 1u;   /* the state machine sits at halt: clear first */
        rp_resync(7u, stat_viol);
        return;
    }
    core1_phase = PH_VIOL;
    server_running = false;
    for (;;) {
        flash_sync();
    }
}

static __force_inline uint32_t rx_get(void)
{
    while (pio0_hw->fstat & FS_RX_EMPTY) {
        flash_sync();
        if (pio0_hw->irq & 1u) {
            on_viol();
        }
    }
    return pio0_hw->rxf[SM];
}

/* Console input credit: one byte per 8 us, at most CON_FIFO waiting. */
static __force_inline uint32_t con_waiting(void)
{
    uint32_t now = timer_hw->timerawl;
    uint32_t dt = now - con_t;
    uint32_t n = dt >> 3;

    if (n != 0) {
        con_t += n << 3;
        uint32_t a = con_avail + n;
        if (a - con_deliv > CON_FIFO || a < con_avail) {
            a = con_deliv + CON_FIFO;
        }
        con_avail = a;
    }
    return con_avail - con_deliv;
}

static __force_inline uint32_t dev_access(uint32_t off, bool wr, uint32_t wdata, uint32_t *rdata)
{
    uint32_t st = ST_OK;

    if (off == 0x04u) {
        if (wr) {
            dev_scratch = wdata;
        } else {
            *rdata = dev_scratch;
        }
    } else if (off == 0x14u) {
        if (wr) {
            if ((wdata & 0xFFu) != (h(con_echo, 0xC0DE0000u) & 0xFFu)) {
                con_bad++;
            }
            con_echo++;
        } else if (con_waiting() != 0) {
            *rdata = h(con_deliv, 0xC0DE0000u) & 0xFFu;
            con_deliv++;
        } else {
            *rdata = 0x100u;
        }
    } else if (off == 0x10u) {
        if (wr) {
            st = ST_BUS_ERROR;
        } else {
            *rdata = 2u | ((con_waiting() != 0) ? 1u : 0u);
        }
    } else if (off == 0x0Cu) {
        if (wr) {
            st = ST_BUS_ERROR;
        } else {
            *rdata = timer_hw->timerawl;
        }
    } else if (off == 0x18u) {
        if (wr) {
            if (sio_hw->gpio_out & IRQ_BIT) {
                uint32_t lat = timer_hw->timerawl - irq_t;
                sio_hw->gpio_clr = IRQ_BIT;
                stat_irq++;
                stat_irq_sum += lat;
                if (lat > stat_irq_max) {
                    stat_irq_max = lat;
                }
            }
        } else {
            *rdata = (sio_hw->gpio_out & IRQ_BIT) ? 1u : 0u;
        }
    } else if (off == 0x2Cu) {
        if (wr) {
            st = ST_BUS_ERROR;
        } else {
            *rdata = (disk_done << 16) | (disk_err << 1) | disk_busy;
        }
    } else if (off == 0x20u) {
        if (wr) {
            disk_sec = wdata;
        } else {
            *rdata = disk_sec;
        }
    } else if (off == 0x24u) {
        if (wr) {
            disk_buf = wdata;
        } else {
            *rdata = disk_buf;
        }
    } else if (off == 0x28u) {
        if (wr) {
            disk_cmd = wdata;
            if (disk_busy || disk_sec >= DISK_SECS || (disk_buf & 3u) != 0
                || disk_buf > RAM_SIZE - SEC_SIZE || (wdata != 1u && wdata != 2u)) {
                disk_err = 1u;
            } else {
                disk_busy = 1u;
                disk_go_sec = disk_sec;
                disk_go_buf = disk_buf;
                __dmb();
                disk_go_cmd = wdata;
            }
        } else {
            *rdata = disk_cmd;
        }
    } else if (off == 0x00u) {
        if (wr) {
            st = ST_BUS_ERROR;
        } else {
            *rdata = DEV_ID;
        }
    } else if (off == 0x08u) {
        if (wr) {
            st = ST_BUS_ERROR;
        } else {
            *rdata = stat_tx;
        }
    } else {
        st = ST_BUS_ERROR;
    }
    return st;
}

static void __not_in_flash_func(server)(void)
{
    uint32_t next_addr[2] = {0, 0};
    uint32_t next_valid[2] = {0, 0};
    uint32_t cmd[7];
    /* The copy of the last answer, for repeats (v1). Core 1 restarts with
     * every server start, which is every chip reset, so it starts empty. */
    bool last_ok = false;
    uint32_t last_hdr = 0;
    uint32_t last_addr = 0;
    uint32_t last_wdata = 0;
    uint32_t last_st = 0;
    uint32_t last_rd = 0;
    uint32_t inj_n = 0;

    systick_hw->rvr = 0x00FFFFFFu;
    systick_hw->cvr = 0;
    systick_hw->csr = 5u;  /* processor clock, no interrupt */
    uint32_t p_end = systick_hw->cvr;
    uint32_t p_hdr = 0;
    uint32_t p_rcv = 0;
    uint32_t p_cmp = 0;

    pio0_hw->txf[SM] = CMD_RECV;  /* the first header */

    for (;;) {
        core1_phase = PH_HDR;
        uint32_t hdr = rx_get() & 0xFFu;
        uint32_t t_last = timer_hw->timerawl;  /* the chip's last sign of life */
        if (prof_on) {
            p_hdr = systick_hw->cvr;
        }
        core1_phase = PH_RECV;

        uint32_t inj = 0;
        if (inj_every != 0u) {
            inj_n++;
            if (inj_n >= inj_every) {
                inj_n = 0;
                inj = inj_mode;
            }
        }
        if (inj == 3u) {  /* stall after the header */
            stat_inj++;
            rp_resync(1u, hdr);
            continue;
        }

        uint32_t seq = hdr & 1u;
        bool wr = (hdr & 0x80u) != 0;
        bool nx = (hdr & 0x40u) != 0;
        uint32_t sc = (hdr >> 4) & 3u;
        uint32_t kind = (hdr >> 3) & 1u;  /* 1 fetch, 0 data */
        bool kernel = (hdr & 0x04u) != 0;
        uint32_t size = (sc == 0u) ? 1u : (sc == 1u) ? 2u : (sc == 2u) ? 4u : 0u;
        uint32_t nhead = nx ? 0u : 4u;
        uint32_t nrecv = nhead + (wr ? size : 0u);
        uint32_t addr = 0;
        uint32_t wdata = 0;
        uint32_t sent = 0;
        uint32_t got = 0;
        bool lost = false;

        while (got < nrecv) {
            uint32_t fs = pio0_hw->fstat;
            if (sent < nrecv && !(fs & FS_TX_FULL)) {
                pio0_hw->txf[SM] = CMD_RECV;
                sent++;
            }
            if (!(fs & FS_RX_EMPTY)) {
                uint32_t b = pio0_hw->rxf[SM] & 0xFFu;
                if (got < nhead) {
                    addr |= b << (8u * got);
                } else {
                    wdata |= b << (8u * (got - nhead));
                }
                got++;
                t_last = timer_hw->timerawl;
            } else {
                flash_sync();
                if (timer_hw->timerawl - t_last > t_rp_us) {
                    lost = true;
                    break;
                }
            }
        }
        if (lost) {
            stat_rp_to++;
            rp_resync(2u, got | (nrecv << 8) | (hdr << 16));
            continue;
        }
        if (inj == 1u) {  /* drop: nothing done, no answer */
            stat_inj++;
            rp_resync(3u, hdr);
            continue;
        }

        /* The chip's next toggle hands uio over; the PIO holds it until
         * the status command below. */
        if (prof_on) {
            p_rcv = systick_hw->cvr;
        }
        core1_phase = PH_ANSWER;
        uint32_t st = ST_OK;
        uint32_t rdata = 0;
        if (last_ok && hdr == last_hdr && addr == last_addr && wdata == last_wdata) {
            /* A repeat: the chip never got the answer. Same answer, done
             * once. */
            st = last_st;
            rdata = last_rd;
            stat_replay++;
        } else {
            uint32_t addr_rx = addr;
            if (nx) {
                if (!next_valid[kind]) {
                    st = ST_BAD_NEXT;
                } else {
                    addr = next_addr[kind];
                }
            }
            if (st == ST_OK && size == 0u) {
                st = ST_BUS_ERROR;
            }
            if (st == ST_OK && (addr & (size - 1u)) != 0) {
                st = ST_MISALIGNED;
            }
            if (st == ST_OK) {
                if (addr + size <= RAM_SIZE) {
                    if (!kernel && addr + size > KERNEL_BASE) {
                        st = ST_PROTECTION;
                    } else if (wr) {
                        if (size == 4u) {
                            *(uint32_t *)&ram[addr] = wdata;
                        } else if (size == 2u) {
                            *(uint16_t *)&ram[addr] = (uint16_t)wdata;
                        } else {
                            ram[addr] = (uint8_t)wdata;
                        }
                        stat_ram_wr++;
                    } else {
                        if (size == 4u) {
                            rdata = *(uint32_t *)&ram[addr];
                        } else if (size == 2u) {
                            rdata = *(uint16_t *)&ram[addr];
                        } else {
                            rdata = ram[addr];
                        }
                        stat_ram_rd++;
                    }
                } else if (addr >= BIOS_BASE && addr + size <= BIOS_BASE + BIOS_SIZE) {
                    if (wr) {
                        st = ST_BUS_ERROR;
                    } else {
                        const uint8_t *p = &rom[addr - BIOS_BASE];
                        if (size == 4u) {
                            rdata = *(const uint32_t *)p;
                        } else if (size == 2u) {
                            rdata = *(const uint16_t *)p;
                        } else {
                            rdata = *p;
                        }
                        stat_rom++;
                    }
                } else if (addr >= DEV_BASE && addr < DEV_BASE + DEV_SIZE) {
                    if (size != 4u) {
                        st = ST_BUS_ERROR;
                    } else if (kind == 1u) {
                        st = ST_PROTECTION;
                    } else {
                        st = dev_access(addr - DEV_BASE, wr, wdata, &rdata);
                        stat_dev++;
                    }
                } else {
                    st = ST_BUS_ERROR;
                }
            }
            if (st == ST_OK) {
                next_addr[kind] = addr + size;
                next_valid[kind] = 1u;
                if (kind) {
                    stat_fetch++;
                }
            } else {
                next_valid[kind] = 0u;
                stat_faults++;
            }
            last_ok = true;
            last_hdr = hdr;
            last_addr = addr_rx;
            last_wdata = wdata;
            last_st = st;
            last_rd = rdata;
        }

        if (inj == 4u) {  /* done, but the answer is lost */
            stat_inj++;
            rp_resync(4u, hdr);
            continue;
        }
        if (inj == 5u) {  /* done, answer late */
            stat_inj++;
            uint32_t t0 = timer_hw->timerawl;
            while (timer_hw->timerawl - t0 <= 2u * t_rp_us) {
                flash_sync();
            }
        }
        /* Stale check: a chip that has waited this long may give up and
         * retry at any moment; driving now could meet its next header. */
        if (timer_hw->timerawl - t_last > t_rp_us) {
            stat_stale++;
            rp_resync(5u, hdr);
            continue;
        }

        /* Status, the read bytes, the release, and a receive for the next
         * header; every one of these events also pushes a sample that is
         * thrown away, except the next header's. */
        uint32_t stb = (seq ? 0x80u : 0x40u) | st;
        if (inj == 2u) {
            stat_inj++;
            stb = (seq ? 0x40u : 0x80u) | st;  /* wrong SEQ */
        }
        uint32_t nrd = (st == ST_OK && !wr) ? size : 0u;
        uint32_t ncmd = 0;
        cmd[ncmd++] = CMD_DRIVE(stb);
        for (uint32_t i = 0; i < nrd; i++) {
            cmd[ncmd++] = CMD_DRIVE((rdata >> (8u * i)) & 0xFFu);
        }
        cmd[ncmd++] = CMD_RECV;  /* release */
        cmd[ncmd++] = CMD_RECV;  /* next header */
        uint32_t drop = nrd + 2u;
        bool viol = false;
        uint32_t i = 0;
        if (prof_on) {
            p_cmp = systick_hw->cvr;
        }
        while (i < ncmd || drop != 0) {
            uint32_t fs = pio0_hw->fstat;
            if (i < ncmd && !(fs & FS_TX_FULL)) {
                pio0_hw->txf[SM] = cmd[i++];
            }
            if (drop != 0) {
                if (!(fs & FS_RX_EMPTY)) {
                    (void)pio0_hw->rxf[SM];
                    drop--;
                    t_last = timer_hw->timerawl;
                } else if (timer_hw->timerawl - t_last > t_rp_us) {
                    lost = true;
                    break;
                }
            }
            if (pio0_hw->irq & 1u) {
                on_viol();
                viol = true;
                break;
            }
            flash_sync();
        }
        if (viol) {
            continue;
        }
        if (lost) {
            stat_rp_to++;
            rp_resync(6u, drop | (i << 8) | (ncmd << 16) | (hdr << 24));
            continue;
        }

        uint32_t n = stat_tx;
        uint32_t k = n & (TRACE_N - 1u);
        trace_a[k] = addr | (hdr << 24);
        trace_s[k] = stb;
        trace_d[k] = wr ? wdata : rdata;
        trace_t[k] = timer_hw->timerawl;
        stat_tx = n + 1u;
        if (prof_on) {
            uint32_t now = systick_hw->cvr;
            prof_idle += (p_end - p_hdr) & 0x00FFFFFFu;
            prof_recv += (p_hdr - p_rcv) & 0x00FFFFFFu;
            prof_cmp += (p_rcv - p_cmp) & 0x00FFFFFFu;
            prof_resp += (p_cmp - now) & 0x00FFFFFFu;
            prof_n++;
            p_end = now;
        } else {
            p_end = systick_hw->cvr;
        }
    }
}

/* Core 0. */

/* Take the flash from core 1 (which may be serving). */
static void flash_begin(void)
{
    flash_req = 1u;
    __dmb();
    if (server_running) {
        while (!flash_ack) {
            tight_loop_contents();
        }
    }
    __dmb();
}

static void flash_end(void)
{
    __dmb();
    flash_req = 0u;
    if (server_running) {
        while (flash_ack) {
            tight_loop_contents();
        }
    }
}

static void flash_erase(uint32_t off, uint32_t len)
{
    flash_begin();
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(off, len);
    restore_interrupts(irq);
    flash_end();
}

static void flash_program(uint32_t off, const uint8_t *src, uint32_t len)
{
    flash_begin();
    uint32_t irq = save_and_disable_interrupts();
    flash_range_program(off, src, len);
    restore_interrupts(irq);
    flash_end();
}

static void disk_work(void)
{
    uint32_t cmd = disk_go_cmd;

    if (cmd == 0) {
        return;
    }
    __dmb();
    uint32_t off = DISK_OFF + disk_go_sec * SEC_SIZE;
    uint8_t *buf = &ram[disk_go_buf];
    const uint8_t *xip = (const uint8_t *)(XIP_NOCACHE_NOALLOC_BASE + off);
    uint32_t t0 = time_us_32();
    if (cmd == 2u) {
        flash_erase(off, SEC_SIZE);
        flash_program(off, buf, SEC_SIZE);
        if (memcmp(xip, buf, SEC_SIZE) != 0) {
            disk_err = 1u;
            stat_disk_bad++;
        }
        uint32_t dt = time_us_32() - t0;
        stat_disk_w++;
        if (dt > stat_disk_wmax) {
            stat_disk_wmax = dt;
        }
    } else {
        memcpy(buf, xip, SEC_SIZE);
        uint32_t dt = time_us_32() - t0;
        stat_disk_r++;
        if (dt > stat_disk_rmax) {
            stat_disk_rmax = dt;
        }
    }
    disk_done++;
    __dmb();
    disk_go_cmd = 0;
    disk_busy = 0;
}

static uint32_t rnd_next(void)
{
    rnd ^= rnd << 13;
    rnd ^= rnd >> 17;
    rnd ^= rnd << 5;
    return rnd;
}

static bool chip_in_reset(void);

static void irq_work(void)
{
    if (!irq_gen || !server_running || chip_in_reset()) {
        irq_next = 0;
        return;
    }
    if (sio_hw->gpio_out & IRQ_BIT) {
        irq_next = 0;
        return;
    }
    uint32_t now = time_us_32();
    if (irq_next == 0) {
        irq_next = now + 100u + rnd_next() % 901u;
        if (irq_next == 0) {
            irq_next = 1;
        }
    } else if ((int32_t)(now - irq_next) >= 0) {
        irq_t = time_us_32();
        __dmb();
        sio_hw->gpio_set = IRQ_BIT;
        stat_irq_raised++;
        irq_next = 0;
    }
}

static void watch_work(void)
{
    while (!pio_sm_is_rx_fifo_empty(pio1, 0)) {
        uint32_t v = pio1->rxf[0];
        uint32_t k = gl_n & (GL_N - 1u);
        gl_t[k] = time_us_32();
        gl_k[k] = v;
        gl_n++;
        if (v) {
            req_gl_lo++;
        } else {
            req_gl_hi++;
        }
    }
}

static void watch_start(void)
{
    pio_sm_set_enabled(pio1, 0, false);
    pio_sm_config c = reqwatch_program_get_default_config(watch_offset);
    sm_config_set_in_pins(&c, PIN_REQ);
    sm_config_set_jmp_pin(&c, PIN_REQ);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(pio1, 0, watch_offset, &c);
    gl_n = 0;
    req_gl_hi = 0;
    req_gl_lo = 0;
    pio_sm_set_enabled(pio1, 0, true);
}

static void pins_all_inputs(void)
{
    for (uint pin = 0; pin < NUM_PINS; pin++) {
        gpio_init(pin);
        gpio_set_pulls(pin, false, true);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
    }
}

static void clk_pin_stop(void)
{
    pwm_set_enabled(pwm_gpio_to_slice_num(PIN_CLK), false);
    gpio_init(PIN_CLK);
    gpio_put(PIN_CLK, false);
    gpio_set_dir(PIN_CLK, GPIO_OUT);
    clk_div = 0;
}

static void clk_pin_start(uint32_t div)
{
    uint slice = pwm_gpio_to_slice_num(PIN_CLK);

    pwm_set_enabled(slice, false);
    pwm_set_clkdiv(slice, 1.0f);
    pwm_set_wrap(slice, (uint16_t)(div - 1u));
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_CLK), (uint16_t)(div / 2u));
    gpio_set_drive_strength(PIN_CLK, clk_drive);
    gpio_set_slew_rate(PIN_CLK, clk_slew);
    gpio_set_function(PIN_CLK, GPIO_FUNC_PWM);
    pwm_set_counter(slice, 0);
    pwm_set_enabled(slice, true);
    clk_div = div;
}

static void apply_drive(void)
{
    for (uint pin = UIO_SHIFT; pin < UIO_SHIFT + 8u; pin++) {
        gpio_set_drive_strength(pin, out_drive);
        gpio_set_slew_rate(pin, out_slew);
    }
    const uint outs[] = {PIN_ACK, PIN_IRQ, PIN_PDRV};
    for (uint i = 0; i < 3u; i++) {
        gpio_set_drive_strength(outs[i], out_drive);
        gpio_set_slew_rate(outs[i], out_slew);
    }
}

static void server_stop(void)
{
    multicore_reset_core1();
    server_running = false;
    core1_phase = PH_STOPPED;
    pio_sm_set_enabled(pio0, SM, false);
    pio_sm_set_enabled(pio1, 0, false);
    /* uio and the PIO outputs back to plain inputs and low outputs. */
    for (uint pin = UIO_SHIFT; pin < UIO_SHIFT + 8u; pin++) {
        gpio_init(pin);
        gpio_set_pulls(pin, false, true);
    }
    gpio_init(PIN_ACK);
    gpio_put(PIN_ACK, false);
    gpio_set_dir(PIN_ACK, GPIO_OUT);
    gpio_init(PIN_PDRV);
    gpio_put(PIN_PDRV, false);
    gpio_set_dir(PIN_PDRV, GPIO_OUT);
    gpio_put(PIN_IRQ, false);
    flash_req = 0;
    flash_ack = 0;
}

static void pio_setup(void)
{
    pio_sm_config c = rpbus_program_get_default_config(pio_offset);

    sm_config_set_in_pins(&c, UIO_SHIFT);
    sm_config_set_out_pins(&c, UIO_SHIFT, 8);
    sm_config_set_set_pins(&c, PIN_ACK, 1);
    sm_config_set_sideset_pins(&c, PIN_PDRV);
    sm_config_set_jmp_pin(&c, PIN_CDRV);
    sm_config_set_in_shift(&c, false, true, 8);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(pio0, SM, pio_offset, &c);

    uint32_t mask = UIO_MASK | ACK_BIT | PDRV_BIT;
    pio_sm_set_pins_with_mask(pio0, SM, 0, mask);
    pio_sm_set_pindirs_with_mask(pio0, SM, ACK_BIT | PDRV_BIT, mask);
    for (uint pin = UIO_SHIFT; pin < UIO_SHIFT + 8u; pin++) {
        pio_gpio_init(pio0, pin);
        gpio_set_pulls(pin, false, true);
    }
    pio_gpio_init(pio0, PIN_ACK);
    pio_gpio_init(pio0, PIN_PDRV);
    pio0_hw->irq = 1u;

    ins_osr0 = pio_encode_mov(pio_osr, pio_null);
    ins_rel = pio_encode_out(pio_pindirs, 8) | pio_encode_sideset_opt(1, 0);
    ins_rise = pio_encode_jmp(pio_offset + rpbus_offset_rise);
    ins_fall = pio_encode_jmp(pio_offset + rpbus_offset_fall);
    ins_ack0 = pio_encode_set(pio_pins, 0);
    ins_ack1 = pio_encode_set(pio_pins, 1);
}

static void server_start(void)
{
    server_stop();
    /* The chip is held in reset first, so it is not driving uio. */
    gpio_put(PIN_RST, false);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_put(PIN_IRQ, false);
    gpio_set_dir(PIN_IRQ, GPIO_OUT);
    pio_setup();
    apply_drive();

    stat_tx = 0;
    stat_faults = 0;
    stat_viol = 0;
    stat_ram_rd = 0;
    stat_ram_wr = 0;
    stat_fetch = 0;
    stat_rom = 0;
    stat_dev = 0;
    stat_replay = 0;
    stat_resync = 0;
    stat_rp_to = 0;
    stat_stale = 0;
    stat_inj = 0;
    stat_ackfix = 0;
    rec_n = 0;
    stat_irq = 0;
    stat_irq_raised = 0;
    stat_irq_max = 0;
    stat_irq_sum = 0;
    stat_disk_w = 0;
    stat_disk_r = 0;
    stat_disk_bad = 0;
    stat_disk_wmax = 0;
    stat_disk_rmax = 0;
    con_t = timer_hw->timerawl;
    con_avail = 0;
    con_deliv = 0;
    con_echo = 0;
    con_bad = 0;
    irq_next = 0;
    memset(trace_t, 0, sizeof(trace_t));

    prof_idle = 0;
    prof_recv = 0;
    prof_cmp = 0;
    prof_resp = 0;
    prof_n = 0;

    watch_start();
    pio_sm_set_enabled(pio0, SM, true);
    server_running = true;
    multicore_launch_core1(server);
}

static void disk_reset(void)
{
    disk_sec = 0;
    disk_buf = 0;
    disk_cmd = 0;
    disk_busy = 0;
    disk_err = 0;
    disk_done = 0;
    disk_go_cmd = 0;
}

static bool chip_in_reset(void)
{
    return (sio_hw->gpio_out & RST_BIT) == 0;
}

static void rom_fill(uint32_t seed)
{
    for (uint32_t o = 0; o < BIOS_SIZE; o += 4u) {
        uint32_t w = h(o, seed);
        memcpy(&rom[o], &w, 4);
    }
    printf("ok f %08lx bytes=%lu\n", (unsigned long)seed, (unsigned long)BIOS_SIZE);
}

static void rom_check(uint32_t seed)
{
    uint32_t bad = 0;
    uint32_t first = 0xFFFFFFFFu;

    for (uint32_t o = 0; o < BIOS_SIZE; o += 4u) {
        uint32_t w;
        memcpy(&w, &rom[o], 4);
        if (w != h(o, seed)) {
            if (bad == 0) {
                first = o;
            }
            bad++;
        }
    }
    printf("ok v %08lx bad=%lu first=%lx bytes=%lu\n", (unsigned long)seed, (unsigned long)bad,
           (unsigned long)first, (unsigned long)BIOS_SIZE);
}

static void print_stats(void)
{
    printf("ok s tx=%lu faults=%lu viol=%lu running=%d phase=%lu div=%lu "
           "ram_rd=%lu ram_wr=%lu fetch=%lu rom=%lu dev=%lu "
           "replay=%lu resync=%lu rp_to=%lu stale=%lu inj=%lu ackfix=%lu t_rp_us=%lu "
           "irq=%lu irq_raised=%lu irq_max_us=%lu irq_sum_us=%lu req_gl_hi=%lu req_gl_lo=%lu "
           "disk_w=%lu disk_r=%lu disk_bad=%lu disk_err=%lu disk_wmax_us=%lu disk_rmax_us=%lu "
           "con_deliv=%lu con_echo=%lu con_bad=%lu "
           "pio_pc=%lu flevel=%lx pins=%lx irqgen=%d us=%lu\n",
           (unsigned long)stat_tx, (unsigned long)stat_faults, (unsigned long)stat_viol,
           server_running ? 1 : 0, (unsigned long)core1_phase, (unsigned long)clk_div,
           (unsigned long)stat_ram_rd, (unsigned long)stat_ram_wr, (unsigned long)stat_fetch,
           (unsigned long)stat_rom, (unsigned long)stat_dev,
           (unsigned long)stat_replay, (unsigned long)stat_resync, (unsigned long)stat_rp_to,
           (unsigned long)stat_stale, (unsigned long)stat_inj, (unsigned long)stat_ackfix,
           (unsigned long)t_rp_us,
           (unsigned long)stat_irq, (unsigned long)stat_irq_raised, (unsigned long)stat_irq_max,
           (unsigned long)stat_irq_sum, (unsigned long)req_gl_hi, (unsigned long)req_gl_lo,
           (unsigned long)stat_disk_w, (unsigned long)stat_disk_r, (unsigned long)stat_disk_bad,
           (unsigned long)disk_err, (unsigned long)stat_disk_wmax, (unsigned long)stat_disk_rmax,
           (unsigned long)con_deliv, (unsigned long)con_echo, (unsigned long)con_bad,
           (unsigned long)(pio0_hw->sm[SM].addr - pio_offset), (unsigned long)pio0_hw->flevel,
           (unsigned long)(sio_hw->gpio_in & 0xFFFFu), irq_gen ? 1 : 0,
           (unsigned long)timer_hw->timerawl);
}

static bool parse_drive(const char *s, enum gpio_drive_strength *ds, enum gpio_slew_rate *sr)
{
    char *end = NULL;
    unsigned long ma = strtoul(s, &end, 10);
    unsigned long sl = (end && *end == ' ') ? strtoul(end + 1, NULL, 10) : 9;

    if ((ma != 2 && ma != 4 && ma != 8 && ma != 12) || sl > 1) {
        return false;
    }
    *ds = (ma == 2) ? GPIO_DRIVE_STRENGTH_2MA : (ma == 4) ? GPIO_DRIVE_STRENGTH_4MA
          : (ma == 8) ? GPIO_DRIVE_STRENGTH_8MA : GPIO_DRIVE_STRENGTH_12MA;
    *sr = sl ? GPIO_SLEW_RATE_FAST : GPIO_SLEW_RATE_SLOW;
    return true;
}

static void handle_line(const char *line)
{
    char *end = NULL;

    if (strcmp(line, "i") == 0) {
        printf("ok i bench_mem v1 sys=%lu ram=%lu\n", (unsigned long)clock_get_hz(clk_sys),
               (unsigned long)RAM_SIZE);
    } else if (strcmp(line, "g") == 0) {
        server_start();
        printf("ok g\n");
    } else if (line[0] == 'd' && line[1] == ' ') {
        if (!parse_drive(line + 2, &out_drive, &out_slew)) {
            printf("err d\n");
        } else {
            apply_drive();
            printf("ok %s\n", line);
        }
    } else if (line[0] == 'k' && line[1] == ' ') {
        if (!parse_drive(line + 2, &clk_drive, &clk_slew)) {
            printf("err k\n");
        } else {
            gpio_set_drive_strength(PIN_CLK, clk_drive);
            gpio_set_slew_rate(PIN_CLK, clk_slew);
            printf("ok %s\n", line);
        }
    } else if (line[0] == 'c' && line[1] == ' ') {
        unsigned long div = strtoul(line + 2, &end, 10);
        if (end == line + 2 || *end != '\0' || div == 1 || div > 65535) {
            printf("err c\n");
        } else if (gpio_is_dir_out(PIN_RST) && !chip_in_reset()) {
            printf("err chip not in reset\n");
        } else if (div == 0) {
            clk_pin_stop();
            printf("ok c 0\n");
        } else {
            clk_pin_start((uint32_t)div);
            printf("ok c %lu\n", (unsigned long)(clock_get_hz(clk_sys) / div));
        }
    } else if (line[0] == 'S' && line[1] == ' ') {
        unsigned long khz = strtoul(line + 2, &end, 10);
        if (end == line + 2 || *end != '\0' || khz < 50000u || khz > 200000u) {
            printf("err S\n");
        } else if (server_running || !chip_in_reset()) {
            printf("err S needs r 0\n");
        } else if (!set_sys_clock_khz((uint32_t)khz, false)) {
            printf("err S not possible\n");
        } else {
            if (clk_div != 0) {
                clk_pin_start(clk_div);
            }
            printf("ok S %lu\n", (unsigned long)clock_get_hz(clk_sys));
        }
    } else if (strcmp(line, "r 0") == 0) {
        server_stop();
        gpio_put(PIN_RST, false);
        gpio_set_dir(PIN_RST, GPIO_OUT);
        printf("ok r 0\n");
    } else if (strcmp(line, "r 1") == 0) {
        if (!server_running) {
            printf("err server not running\n");
        } else if (clk_div == 0) {
            printf("err clock off\n");
        } else {
            gpio_put(PIN_RST, true);
            printf("ok r 1\n");
        }
    } else if (strcmp(line, "s") == 0) {
        print_stats();
    } else if (strcmp(line, "z") == 0) {
        if (!chip_in_reset()) {
            printf("err chip not in reset\n");
        } else {
            memset(ram, 0, sizeof(ram));
            dev_scratch = 0;
            disk_reset();
            printf("ok z\n");
        }
    } else if ((line[0] == 'f' || line[0] == 'v') && line[1] == ' ') {
        uint32_t seed = (uint32_t)strtoul(line + 2, &end, 16);
        if (end == line + 2 || *end != '\0') {
            printf("err %c\n", line[0]);
        } else if (line[0] == 'v') {
            rom_check(seed);
        } else if (!chip_in_reset()) {
            printf("err chip not in reset\n");
        } else {
            rom_fill(seed);
        }
    } else if (line[0] == 'T' && line[1] == ' ') {
        unsigned long us = strtoul(line + 2, &end, 10);
        if (end == line + 2 || *end != '\0' || us < 10u || us > 100000u) {
            printf("err T\n");
        } else {
            t_fix_us = 0;
            __dmb();
            t_rp_us = (uint32_t)us;
            t_fix_us = 24u * (uint32_t)us;
            printf("ok T %lu fix %lu\n", us, (unsigned long)t_fix_us);
        }
    } else if (line[0] == 'F' && line[1] == ' ') {
        unsigned long m = strtoul(line + 2, &end, 10);
        unsigned long n = (end && *end == ' ') ? strtoul(end + 1, &end, 10) : 0;
        if (m > 5u || *end != '\0') {
            printf("err F\n");
        } else {
            inj_every = 0;
            __dmb();
            inj_mode = (uint32_t)m;
            __dmb();
            inj_every = (m == 0u) ? 0u : (uint32_t)n;
            printf("ok F %lu %lu\n", m, (unsigned long)inj_every);
        }
    } else if (strcmp(line, "V 0") == 0 || strcmp(line, "V 1") == 0) {
        viol_recover = (line[2] == '1');
        printf("ok V %d\n", (int)viol_recover);
    } else if (strcmp(line, "n 0") == 0 || strcmp(line, "n 1") == 0) {
        irq_gen = (line[2] == '1');
        irq_next = 0;
        if (!irq_gen) {
            sio_hw->gpio_clr = IRQ_BIT;
        }
        printf("ok %s\n", line);
    } else if (strcmp(line, "t") == 0) {
        uint32_t n = stat_tx;
        for (uint32_t i = (n > TRACE_N) ? n - TRACE_N : 0u; i < n; i++) {
            uint32_t k = i & (TRACE_N - 1u);
            printf("tr %lu %lu %08lx %02lx %08lx\n", (unsigned long)i,
                   (unsigned long)trace_t[k], (unsigned long)trace_a[k],
                   (unsigned long)trace_s[k], (unsigned long)trace_d[k]);
        }
        printf("ok t %lu\n", (unsigned long)n);
    } else if (strcmp(line, "L") == 0) {
        uint32_t n = rec_n;
        for (uint32_t i = (n > REC_N) ? n - REC_N : 0u; i < n; i++) {
            uint32_t k = i & (REC_N - 1u);
            printf("rec %lu t=%lu w=%08lx i=%08lx tx=%lu\n", (unsigned long)i, (unsigned long)rec_t[k],
                   (unsigned long)rec_w[k], (unsigned long)rec_i[k], (unsigned long)rec_p[k]);
        }
        printf("ok L %lu\n", (unsigned long)n);
    } else if (strcmp(line, "w") == 0) {
        uint32_t n = gl_n;
        for (uint32_t i = (n > GL_N) ? n - GL_N : 0u; i < n; i++) {
            uint32_t k = i & (GL_N - 1u);
            printf("gl %lu %lu %lu\n", (unsigned long)i, (unsigned long)gl_t[k], (unsigned long)gl_k[k]);
        }
        printf("ok w %lu\n", (unsigned long)n);
    } else if (strcmp(line, "p 0") == 0 || strcmp(line, "p 1") == 0) {
        if (line[2] == '1') {
            prof_on = 0;
            __dmb();
            prof_idle = 0;
            prof_recv = 0;
            prof_cmp = 0;
            prof_resp = 0;
            prof_n = 0;
            __dmb();
            prof_on = 1;
        } else {
            prof_on = 0;
        }
        printf("ok %s\n", line);
    } else if (strcmp(line, "P") == 0) {
        printf("ok P n=%lu idle=%llu recv=%llu decide=%llu answer=%llu\n", (unsigned long)prof_n,
               (unsigned long long)prof_idle, (unsigned long long)prof_recv,
               (unsigned long long)prof_cmp, (unsigned long long)prof_resp);
    } else if (strcmp(line, "x") == 0) {
        server_stop();
        clk_pin_stop();
        pins_all_inputs();
        printf("ok x\n");
    } else if (line[0] != '\0') {
        printf("err %s\n", line);
    }
}

int main(void)
{
    char line[32];
    size_t length = 0;

    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(10);
    set_sys_clock_khz(SYS_KHZ, true);

    pins_all_inputs();
    pio_offset = pio_add_program(pio0, &rpbus_program);
    watch_offset = pio_add_program(pio1, &reqwatch_program);
    disk_reset();
    stdio_usb_init();

    for (;;) {
        int c = getchar_timeout_us(0);

        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\r' || c == '\n') {
                line[length] = '\0';
                handle_line(line);
                length = 0;
            } else if (length < sizeof(line) - 1) {
                line[length++] = (char)c;
            }
        }
        disk_work();
        irq_work();
        watch_work();
    }
}
