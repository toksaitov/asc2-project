/*
    Runs a firmware image on the hardware description of the chip and prints
    the trace that `asc_reftrace` prints for the emulator.

    The chip is the Tiny Tapeout top module, and this program is the RP2040 at
    its pins: it drives the clock and the reset, and it answers the requests
    on the bus as `asc2-kb/bus-protocol-v1.md` describes. The memory and the
    devices behind the bus are the ones of the emulator (`asc.h`), so a
    program meets the same machine in both. The CPU, the interrupt
    controller, the timer, and the keyboard controller are the ones of the
    chip.

    The emulator has no clock. Its devices move once per instruction: after
    the CPU has done an instruction, or has entered an exception, or has
    waited one turn while halted, the timer steps once, a byte of the
    keyboard may arrive, the devices of the RP2040 move, and a waiting
    interrupt is taken. The chip takes many clocks for an instruction, and
    not the same number each time. So the program holds the CPU between two
    instructions with the STOP pin, moves the devices there in the order of
    the emulator, and lets the CPU go for one instruction:

        STOP      ~~~~~~~~~~~~~~~~~~~~\_____/~~~~~~~~~~~~~  ...  ~~~~~~~~~~~
        the CPU   between instructions      | an instruction  ...  | between
        here      a row, the devices move                          a row, ...

    The chip is built with `ASC_TEST_TICK` (`asc_trace_top.sv`), so the timer
    steps when this program says so and not once in 256 clocks.

    With `-t` the chip runs on its own time instead, as it does on a board:
    STOP stays low, and the timer steps once in 256 clocks. Interrupts then
    arrive at other moments than in the emulator, so the program prints no
    rows, and only the end of the run and the console output can be compared
    with the emulator's.
*/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <verilated.h>

#include "Vchip.h"

#include "trace_common.h"

#define PROGRAM_NAME "asc_trace"

#define DEFAULT_MAX_CLOCKS 4000000000 // clocks after which a run is given up unless the command line says otherwise
#define RESET_CLOCKS       20    // clocks that the reset lasts; the chip takes a reset of 8 clocks or more
#define BLIND_CLOCKS       11    // clocks of the reset before the registers of the chip have their values: the pins may show anything until then
#define SETTLE_CLOCKS      16    // clocks for a level on an input pin to pass the filters of the chip and reach the CPU
#define LEAVE_CLOCKS       8     // clocks that STOP stays low at most: a CPU that has not moved on by then is not going to
#define KBD_HALF_CLOCKS    8     // clocks that the clock line of the keyboard stays low, and stays high, for each bit
#define KBD_FRAME_BITS     11    // a start bit, eight bits of the byte, a parity bit, and a stop bit
#define KBD_FRAME_CLOCKS   (KBD_FRAME_BITS * 2 * KBD_HALF_CLOCKS)
#define HALT_TURN_CLOCKS   512   // with `-t`: clocks of a halted CPU that count as one turn of the emulator
#define RAND_SEED          1     // seed of the values that the flops of the chip hold before the reset, unless ASC_SEED in the environment gives another

/* Pins of the Chip */

#define PIN_REQ      0x01 // `uo_out[0]`: the strobe of the chip
#define PIN_DRIVES   0x04 // `uo_out[2]`: high while the chip drives the bytes of the bus
#define PIN_FRAME    0x08 // `uo_out[3]`: high while a transaction is open
#define PIN_ACK      0x01 // `ui_in[0]`: the strobe of the RP2040
#define PIN_IRQ      0x02 // `ui_in[1]`: high while a device of the RP2040 asks for an interrupt
#define PIN_KBD_CLK  0x20 // `ui_in[5]`: the clock line of the keyboard, high at rest
#define PIN_KBD_DATA 0x40 // `ui_in[6]`: the data line of the keyboard, high at rest
#define PIN_STOP     0x80 // `ui_in[7]`: while high, the CPU waits between two instructions
#define PIN_DBG_CK   0x04 // `ui_in[2]`: the clock of the debug port
#define PIN_DBG_DI   0x10 // `ui_in[4]`: its data input: high selects bit 0, low the next bit
#define PIN_DBG_DO   0x02 // `uo_out[1]`: the bit of the debug port that is selected

#define DBG_BITS     640  // bits of the registers that the debug port shows
#define DBG_LOCKED   640  // the bit after them: the port is locked
#define DBG_CR0_HIGH 641  // the two bits after that one: the bits C and A of cr0
#define DBG_ALL_BITS 643
#define DBG_CLOCKS   4    // clocks that the test holds a level of the port; the chip takes its inputs through two flops

/* Words of the Register File (`asc_regs.sv`) */

#define WORD_FP  8
#define WORD_KSP 9
#define WORD_LR  10
#define WORD_USP 11

/* Bytes of a Transaction */

#define HDR_WRITE      0x80 // a write; a read otherwise
#define HDR_NEXT       0x40 // a fetch of "the next word"; the request carries no address
#define HDR_SIZE_SHIFT 4    // two bits: 0 for one byte, 1 for two, 2 for four
#define HDR_SIZE_MASK  0x3
#define HDR_FETCH      0x08 // a fetch of an instruction
#define HDR_KERNEL     0x04 // the CPU is in kernel mode
#define HDR_RESERVED   0x02 // always zero
#define HDR_SEQ        0x01 // the sequence bit, which the status repeats

#define STAT_SEQ_0 0x40 // the upper bits of a status that answers a header with the sequence bit clear
#define STAT_SEQ_1 0x80 // and with the sequence bit set

#define BUS_REQ_SIZE_MAX 9 // a header, four bytes of the address, and four of data
#define BUS_ANS_SIZE_MAX 5 // a status and four bytes of data
#define BUS_ATTEMPTS     4 // number of times that the chip sends a transaction before it gives up

typedef struct trace_args
{
    size_t max_retired;            // number of retired instructions after which the tool gives up
    size_t max_clocks;             // number of clocks after which the tool gives up
    bool is_quiet;                 // print no trace rows
    bool is_ram_dirty;             // fill the RAM that the image does not set with `TRACE_DIRTY_RAM_BYTE`
    bool has_own_time;             // the chip runs without STOP, and its timer steps once in 256 clocks
    size_t answer_delay;           // number of clocks that the RP2040 lets pass before it answers a change of REQ
    size_t fault_period;           // every transaction with this number gets no answer at the first attempt (0 for none)
    size_t debug_period;           // after every instruction with this number the debug port is read and compared (0 for never)
    const char *event_file_path;   // scancodes for the keyboard by retirement order
    const char *con_in_file_path;  // bytes that the console receives
    const char *con_out_file_path; // file for the bytes that the program prints on the console
    const char *disk_file_path;    // contents of the disk
    const char *disk_out_file_path; // file for the contents of the disk at the end
    const char *bus_file_path;     // file for the requests that the chip sends on the bus
    const char *in_file_path;      // firmware image to run
} trace_args_t;

/*
    The RP2040's side of the bus. Every change of REQ is an event. In the
    first part of a transaction an event tells that a byte of the request is
    on the bus, and the RP2040 answers it with a change of ACK when it has
    the byte. One more event hands the bus over. Then the RP2040 puts a byte
    of its answer on the bus with each change of ACK, and an event tells that
    the chip has it. The last change of ACK hands the bus back.
*/
typedef enum rp_bus_state
{
    RP_BUS_TAKE,    // the chip sends the bytes of its request
    RP_BUS_HANDOFF, // the request is complete, and the chip is to hand the bus over
    RP_BUS_GIVE,    // the RP2040 sends the bytes of its answer
    RP_BUS_SILENT   // the RP2040 does not answer, and the chip is to give up
} rp_bus_state_t;

typedef struct rp_bus
{
    rp_bus_state_t state;
    bool ack;              // level of ACK
    bool is_driving;       // the RP2040 drives the bytes of the bus
    uint8_t out;           // byte that it drives
    bool req_seen;         // level of REQ after the last event that the RP2040 has handled
    size_t waited;         // clocks that the present event has waited
    bool was_chip_driving; // the chip drove the bytes of the bus on the clock before

    uint8_t req[BUS_REQ_SIZE_MAX]; // the request of the present transaction
    size_t req_cnt;                // bytes of it that have arrived
    size_t req_size;               // bytes that it has in all, known from the header

    uint8_t ans[BUS_ANS_SIZE_MAX]; // the answer
    size_t ans_cnt;                // bytes that it has
    size_t ans_pos;                // index of the byte that is on the bus

    /*
        The request that the answer belongs to. A request that equals it is
        another attempt at the same transaction: the RP2040 sends the answer
        again and does not repeat the access.
    */
    uint8_t last_req[BUS_REQ_SIZE_MAX];
    size_t last_req_cnt; // 0 if there is none

    size_t mute_left;  // attempts at the last transaction that the RP2040 still leaves without an answer
    size_t trans_cnt;  // transactions so far
    size_t fault_cnt;  // answers held back on request so far (`fault_period`)
    size_t repeat_cnt; // answers sent again so far
} rp_bus_t;

static int parse_count(const char *text, size_t *count)
{
    char *endptr = NULL;

    errno = 0;
    unsigned long parsed = strtoul(text, &endptr, 10);
    if (errno != 0 || endptr == text || *endptr != '\0') {
        return -1;
    }

    *count = (size_t) parsed;

    return 0;
}

static int parse_args(int argc, char *argv[], trace_args_t *args)
{
    args->max_retired = TRACE_DEFAULT_MAX_RETIRED;
    args->max_clocks = DEFAULT_MAX_CLOCKS;
    args->is_quiet = false;
    args->is_ram_dirty = false;
    args->has_own_time = false;
    args->answer_delay = 0;
    args->fault_period = 0;
    args->debug_period = 0;
    args->event_file_path = NULL;
    args->con_in_file_path = NULL;
    args->con_out_file_path = NULL;
    args->disk_file_path = NULL;
    args->disk_out_file_path = NULL;
    args->bus_file_path = NULL;
    args->in_file_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-q") == 0) {
            args->is_quiet = true;
        } else if (strcmp(argv[i], "-r") == 0) {
            args->is_ram_dirty = true;
        } else if (strcmp(argv[i], "-t") == 0) {
            args->has_own_time = true;
        } else if (strlen(argv[i]) == 2 && argv[i][0] == '-' && strchr("ncwfgeiodsb", argv[i][1]) != NULL) {
            char option = argv[i][1];

            if (++i >= argc) {
                return -1;
            }

            if (option == 'n') {
                if (parse_count(argv[i], &args->max_retired) != 0 || args->max_retired == 0) {
                    return -1;
                }
            } else if (option == 'c') {
                if (parse_count(argv[i], &args->max_clocks) != 0 || args->max_clocks == 0) {
                    return -1;
                }
            } else if (option == 'w') {
                if (parse_count(argv[i], &args->answer_delay) != 0) {
                    return -1;
                }
            } else if (option == 'f') {
                if (parse_count(argv[i], &args->fault_period) != 0) {
                    return -1;
                }
            } else if (option == 'g') {
                if (parse_count(argv[i], &args->debug_period) != 0) {
                    return -1;
                }
            } else if (option == 'e') {
                args->event_file_path = argv[i];
            } else if (option == 'i') {
                args->con_in_file_path = argv[i];
            } else if (option == 'o') {
                args->con_out_file_path = argv[i];
            } else if (option == 'd') {
                args->disk_file_path = argv[i];
            } else if (option == 's') {
                args->disk_out_file_path = argv[i];
            } else {
                args->bus_file_path = argv[i];
            }
        } else if (args->in_file_path == NULL) {
            args->in_file_path = argv[i];
        } else {
            return -1;
        }
    }

    return args->in_file_path != NULL ? 0 : -1;
}

/* The RP2040's Side of the Bus */

static void rp_bus_init(rp_bus_t *bus)
{
    bus->state = RP_BUS_TAKE;
    bus->ack = false;
    bus->is_driving = false;
    bus->out = 0x00;
    bus->req_seen = false;
    bus->waited = 0;
    bus->was_chip_driving = false;

    bus->req_cnt = 0;
    bus->req_size = 0;
    bus->ans_cnt = 0;
    bus->ans_pos = 0;
    bus->last_req_cnt = 0;

    bus->mute_left = 0;
    bus->trans_cnt = 0;
    bus->fault_cnt = 0;
    bus->repeat_cnt = 0;
}

/*
    Checks the first byte of a request, and finds the number of bytes that
    the request has.
*/
static bool rp_bus_take_header(rp_bus_t *bus)
{
    uint8_t hdr = bus->req[0];
    uint8_t size_code = (hdr >> HDR_SIZE_SHIFT) & HDR_SIZE_MASK;

    bool is_valid = size_code != HDR_SIZE_MASK && !(hdr & HDR_RESERVED);
    if ((hdr & HDR_WRITE) && (hdr & HDR_FETCH)) {
        is_valid = false;
    }
    if ((hdr & HDR_NEXT) && !(hdr & HDR_FETCH)) {
        is_valid = false;
    }
    if (!is_valid) {
        return false;
    }

    bus->req_size = 1;
    if (!(hdr & HDR_NEXT)) {
        bus->req_size += 4;
    }
    if (hdr & HDR_WRITE) {
        bus->req_size += (size_t) 1 << size_code;
    }

    return true;
}

/*
    Finds the answer to the complete request. Returns false if the RP2040 is
    to stay silent.
*/
static bool rp_bus_serve(rp_bus_t *bus, device_t *dev, FILE *bus_file, const trace_args_t *args)
{
    if (bus->last_req_cnt == bus->req_cnt && memcmp(bus->last_req, bus->req, bus->req_cnt) == 0) {
        if (bus->mute_left == 0) {
            ++bus->repeat_cnt;
            return true;
        }

        // After the last attempt the same request is a new transaction
        if (--bus->mute_left == 0) {
            bus->last_req_cnt = 0;
        }

        return false;
    }

    uint8_t hdr = bus->req[0];
    uint8_t size = (uint8_t) (1 << ((hdr >> HDR_SIZE_SHIFT) & HDR_SIZE_MASK));
    const uint8_t *bytes = bus->req + 1;

    mem_access_t access = MEM_READ;
    if (hdr & HDR_FETCH) {
        access = MEM_FETCH;
    } else if (hdr & HDR_WRITE) {
        access = MEM_WRITE;
    }

    uint32_t addr = 0;
    if (!(hdr & HDR_NEXT)) {
        addr = (uint32_t) bytes[0] | ((uint32_t) bytes[1] << 8) | ((uint32_t) bytes[2] << 16) | ((uint32_t) bytes[3] << 24);
        bytes += 4;
    }

    uint32_t data = 0;
    if (hdr & HDR_WRITE) {
        for (uint8_t i = 0; i < size; ++i) {
            data |= (uint32_t) bytes[i] << (8 * i);
        }
    }

    dev->bus_reqs_cnt = 0;
    uint8_t stat = bus_access(dev, addr, size, access, (hdr & HDR_KERNEL) != 0, (hdr & HDR_NEXT) != 0, &data);
    if (bus_file != NULL) {
        size_t printed = 0;
        trace_print_bus_rows(bus_file, dev, &printed);
    }

    memcpy(bus->last_req, bus->req, bus->req_cnt);
    bus->last_req_cnt = bus->req_cnt;
    bus->mute_left = 0;
    ++bus->trans_cnt;

    // The test register `RP_LOSE_ADDR`: the access is done, and no attempt at it gets an answer
    if (stat == BUS_STAT_LINK_ERROR) {
        bus->mute_left = BUS_ATTEMPTS - 1;
        return false;
    }

    bus->ans[0] = (uint8_t) (((hdr & HDR_SEQ) ? STAT_SEQ_1 : STAT_SEQ_0) | stat);
    bus->ans_cnt = 1;
    if (!(hdr & HDR_WRITE) && stat == BUS_STAT_OK) {
        for (uint8_t i = 0; i < size; ++i) {
            bus->ans[bus->ans_cnt++] = (uint8_t) (data >> (8 * i));
        }
    }

    // A fault on request: the first attempt gets no answer, and the next one gets this answer
    if (args->fault_period != 0 && (bus->trans_cnt % args->fault_period) == 0) {
        ++bus->fault_cnt;
        return false;
    }

    return true;
}

/*
    Looks at the pins of the chip after a clock and sets what the RP2040
    drives from then on. Returns false if the chip has broken a rule of the
    bus.
*/
static bool rp_bus_step(rp_bus_t *bus, const Vchip *chip, device_t *dev, FILE *bus_file, const trace_args_t *args)
{
    bool req = (chip->uo_out & PIN_REQ) != 0;
    bool is_chip_driving = chip->uio_oe != 0x00;
    bool was_chip_driving = bus->was_chip_driving;
    bus->was_chip_driving = is_chip_driving;

    if ((is_chip_driving && chip->uio_oe != 0xff) || is_chip_driving != ((chip->uo_out & PIN_DRIVES) != 0)) {
        fputs("The chip drives a part of the bus, or the pin that tells who drives the bus is wrong\n", stderr);
        return false;
    }

    if (is_chip_driving && bus->is_driving) {
        fputs("The chip and the RP2040 drive the bus together\n", stderr);
        return false;
    }

    /*
        FRAME falls in the middle of a transaction: the chip has waited for
        an answer in vain, and now stays quiet for a while before it tries
        again. It takes the level that ACK has by then as the level to
        start from, so the RP2040 makes ACK equal to REQ, as it is between
        two transactions.
    */
    bool is_open = bus->state != RP_BUS_TAKE || bus->req_cnt > 0;
    if (is_open && !(chip->uo_out & PIN_FRAME)) {
        if (bus->state != RP_BUS_SILENT) {
            fputs("The chip has given up on a transaction that the RP2040 was serving\n", stderr);
            return false;
        }

        bus->state = RP_BUS_TAKE;
        bus->req_cnt = 0;
        bus->is_driving = false;
        bus->ack = req;
        bus->req_seen = req;
        bus->waited = 0;

        return true;
    }

    if (req == bus->req_seen || bus->state == RP_BUS_SILENT) {
        return true;
    }

    if (bus->waited < args->answer_delay) {
        ++bus->waited;
        return true;
    }
    bus->waited = 0;
    bus->req_seen = req;

    if (bus->state == RP_BUS_TAKE) {
        if (!is_chip_driving) {
            fputs("The chip does not drive a byte that it sends\n", stderr);
            return false;
        }

        bus->req[bus->req_cnt++] = chip->uio_out;
        if (bus->req_cnt == 1 && !rp_bus_take_header(bus)) {
            fprintf(stderr, "Invalid header of a request: 0x%02x\n", (unsigned) bus->req[0]);
            return false;
        }

        if (bus->req_cnt == bus->req_size) {
            bus->state = RP_BUS_HANDOFF;
        }
        bus->ack = req;
    } else if (bus->state == RP_BUS_HANDOFF) {
        // The RP2040 may drive the bus as soon as it sees the change of REQ
        if (is_chip_driving || was_chip_driving) {
            fputs("The chip has not left the bus a clock before it hands it over\n", stderr);
            return false;
        }

        if (rp_bus_serve(bus, dev, bus_file, args)) {
            bus->state = RP_BUS_GIVE;
            bus->ans_pos = 0;
            bus->out = bus->ans[0];
            bus->is_driving = true;
            bus->ack = req;
        } else {
            bus->state = RP_BUS_SILENT;
        }
    } else {
        // The chip has taken a byte of the answer. After the last one the RP2040 hands the bus back.
        if (++bus->ans_pos < bus->ans_cnt) {
            bus->out = bus->ans[bus->ans_pos];
        } else {
            bus->state = RP_BUS_TAKE;
            bus->req_cnt = 0;
            bus->is_driving = false;
        }
        bus->ack = req;
    }

    return true;
}

/* The Chip */

/*
    A run of the chip: the chip, the RP2040 at its pins, and what this
    program has seen the CPU do.
*/
typedef struct chip_run
{
    Vchip *chip;
    rp_bus_t bus;
    device_t *dev;
    FILE *bus_file;
    const trace_args_t *args;

    /* Pins that the program drives besides the bus */

    bool stop;     // STOP
    bool irq;      // the interrupt line of the RP2040
    bool is_bus_watched; // the RP2040 looks at the bus: not in the first clocks of the reset
    bool kbd_clk;  // the two lines of the keyboard
    bool kbd_data;
    bool tick;     // one step of the timer
    bool dbg_ck;   // the two inputs of the debug port
    bool dbg_di;

    uint16_t kbd_frame; // the bits of the frame that the keyboard sends, the first one in bit 0
    size_t kbd_left;    // clocks left of the frame, or 0 if the keyboard sends nothing

    size_t clocks;     // clocks so far
    int status;        // exit status if the run cannot go on
    bool has_left;     // the CPU has started something since the flag was cleared
    bool has_retired;  // an instruction has retired since the flag was cleared
    uint32_t fetch_ip; // address of the last instruction that the CPU has started to fetch
    size_t debug_reads; // times that the debug port was read and compared
    size_t debug_locked_reads; // of them, times that the port was locked
} chip_run_t;

static void chip_set_inputs(chip_run_t *run)
{
    Vchip *chip = run->chip;

    uint8_t ui_in = 0;
    if (run->bus.ack) {
        ui_in |= PIN_ACK;
    }
    if (run->irq) {
        ui_in |= PIN_IRQ;
    }
    if (run->kbd_clk) {
        ui_in |= PIN_KBD_CLK;
    }
    if (run->kbd_data) {
        ui_in |= PIN_KBD_DATA;
    }
    if (run->stop) {
        ui_in |= PIN_STOP;
    }
    if (run->dbg_ck) {
        ui_in |= PIN_DBG_CK;
    }
    if (run->dbg_di) {
        ui_in |= PIN_DBG_DI;
    }
    chip->ui_in = ui_in;
    chip->test_tick_on = !run->args->has_own_time;
    chip->test_tick = run->tick;

    // The board pulls up a pin of the bus that nobody drives
    if (run->bus.is_driving) {
        chip->uio_in = run->bus.out;
    } else {
        chip->uio_in = (uint8_t) ((chip->uio_out & chip->uio_oe) | ~chip->uio_oe);
    }
}

/*
    Gives the chip one clock and lets the RP2040 look at the bus. Returns
    false, with the exit status set, if the run cannot go on.
*/
static bool chip_clock(chip_run_t *run)
{
    Vchip *chip = run->chip;

    if (run->clocks == run->args->max_clocks) {
        fputs("The clock limit was reached before the program parked the CPU\n", stderr);
        run->status = TRACE_EXIT_NO_END;
        return false;
    }
    ++run->clocks;

    chip_set_inputs(run);
    chip->clk = 0;
    chip->eval();
    chip->clk = 1;
    chip->eval();

    if (run->is_bus_watched && !rp_bus_step(&run->bus, chip, run->dev, run->bus_file, run->args)) {
        fprintf(stderr, "The chip was stopped after %zu clocks\n", run->clocks);
        run->status = TRACE_EXIT_ERROR;
        return false;
    }

    if (!chip->cpu_boundary) {
        run->has_left = true;
    }
    if (chip->cpu_fetch) {
        run->fetch_ip = chip->cpu_ip;
    }
    if (chip->cpu_retired) {
        run->has_retired = true;
    }

    return true;
}

static bool chip_clocks(chip_run_t *run, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (!chip_clock(run)) {
            return false;
        }
    }

    return true;
}

/* Runs the chip until the CPU is between two instructions. */
static bool chip_wait_boundary(chip_run_t *run)
{
    while (!run->chip->cpu_boundary) {
        if (!chip_clock(run)) {
            return false;
        }
    }

    return true;
}

static bool chip_reset(chip_run_t *run)
{
    // Until the reset has reached every register, the flops hold random values and the RP2040 does not look at the bus
    run->chip->rst_n = 0;
    run->is_bus_watched = false;
    if (!chip_clocks(run, BLIND_CLOCKS)) {
        return false;
    }
    run->is_bus_watched = true;
    if (!chip_clocks(run, RESET_CLOCKS - BLIND_CLOCKS)) {
        return false;
    }
    run->chip->rst_n = 1;

    // The reset leaves the chip a few clocks later, and the CPU waits for STOP to fall
    return chip_clocks(run, SETTLE_CLOCKS) && chip_wait_boundary(run);
}

/*
    Lets the CPU do one step: an instruction, with the entry of an interrupt
    before it if one waits, or the entry of an exception. A CPU that is
    halted and has no interrupt to take stays where it is, and so does a CPU
    after a double fault.
*/
static bool chip_step(chip_run_t *run)
{
    run->has_left = false;
    run->has_retired = false;

    run->stop = false;
    for (size_t i = 0; i < LEAVE_CLOCKS && !run->has_left; ++i) {
        if (!chip_clock(run)) {
            return false;
        }
    }
    run->stop = true;

    // STOP reaches the CPU two clocks after the pin; the step takes many more
    return chip_clocks(run, SETTLE_CLOCKS) && chip_wait_boundary(run);
}

/* Steps the timer of the chip once. */
static bool chip_tick(chip_run_t *run)
{
    run->tick = true;
    bool is_ok = chip_clock(run);
    run->tick = false;

    return is_ok;
}

/*
    Starts a byte as a keyboard sends it: a start bit of 0, the eight bits
    from the lowest one up, a parity bit that makes the number of ones odd,
    and a stop bit of 1.
*/
static void chip_start_scancode(chip_run_t *run, uint8_t scancode)
{
    bool parity = true;
    for (size_t i = 0; i < 8; ++i) {
        if (scancode & (1 << i)) {
            parity = !parity;
        }
    }

    run->kbd_frame = (uint16_t) (((uint16_t) scancode << 1) | (parity ? 1 << 9 : 0) | (1 << 10));
    run->kbd_left = KBD_FRAME_CLOCKS;
}

/*
    Sets the two lines of the keyboard for the next clock. Each bit is on the
    data line while the clock line is low and then high, `KBD_HALF_CLOCKS`
    clocks each.
*/
static void chip_step_keyboard(chip_run_t *run)
{
    if (run->kbd_left == 0) {
        run->kbd_clk = true;
        run->kbd_data = true;

        return;
    }

    size_t pos = KBD_FRAME_CLOCKS - run->kbd_left;
    run->kbd_data = (run->kbd_frame >> (pos / (2 * KBD_HALF_CLOCKS))) & 1;
    run->kbd_clk = (pos % (2 * KBD_HALF_CLOCKS)) >= KBD_HALF_CLOCKS;
    --run->kbd_left;
}

/* Sends a byte of the keyboard while the CPU waits. */
static bool chip_send_scancode(chip_run_t *run, uint8_t scancode)
{
    chip_start_scancode(run, scancode);
    while (run->kbd_left > 0) {
        chip_step_keyboard(run);
        if (!chip_clock(run)) {
            return false;
        }
    }
    chip_step_keyboard(run);

    return true;
}

/* Copies the registers of the CPU into the structure of the emulator. */
static void chip_read_cpu(chip_run_t *run, cpu_t *cpu)
{
    Vchip *chip = run->chip;

    cpu_init(cpu);

    cpu->r0 = chip->cpu_regs[0];
    cpu->r1 = chip->cpu_regs[1];
    cpu->r2 = chip->cpu_regs[2];
    cpu->r3 = chip->cpu_regs[3];
    cpu->r4 = chip->cpu_regs[4];
    cpu->r5 = chip->cpu_regs[5];
    cpu->r6 = chip->cpu_regs[6];
    cpu->r7 = chip->cpu_regs[7];
    cpu->fp = chip->cpu_regs[WORD_FP];
    cpu->ksp = chip->cpu_regs[WORD_KSP];
    cpu->usp = chip->cpu_regs[WORD_USP];
    cpu->lr = chip->cpu_regs[WORD_LR];
    cpu->ip = chip->cpu_ip;
    cpu->flags = chip->cpu_flags;

    cpu->ctrl = chip->cpu_cr0;
    cpu->saved_ip = chip->cpu_cr1;
    cpu->fault_addr = chip->cpu_cr2;
    cpu->pt_root = chip->cpu_cr3;
    cpu->saved_flags = chip->cpu_cr4;
    cpu->cause = chip->cpu_cr5;

    cpu->halted = chip->cpu_halted;
    cpu->double_fault = chip->cpu_double_fault;
    cpu->inst = chip->cpu_inst;
}

/*
    Reads the debug port at its three pins, as the RP2040 does, while STOP
    holds the CPU, and compares what it shows with the registers that the
    test sees inside the chip: `ip`, the twelve words of the register file,
    `cr1` to `cr4`, and the bits A and C of `cr0`. A port that the kernel
    has locked (`cr8`) must show zeros in their place and a 1 in the bit
    that tells of the lock, and an open port a 0 there. Returns false, with the exit status set, if a bit differs or
    the run cannot go on.
*/
static bool chip_check_debug(chip_run_t *run, const cpu_t *cpu, size_t order)
{
    uint32_t words[DBG_ALL_BITS / 32 + 1] = { 0 };

    for (size_t i = 0; i < DBG_ALL_BITS; ++i) {
        run->dbg_di = i == 0;
        if (!chip_clocks(run, DBG_CLOCKS)) {
            return false;
        }
        run->dbg_ck = true;
        if (!chip_clocks(run, DBG_CLOCKS)) {
            return false;
        }
        run->dbg_ck = false;
        if (!chip_clocks(run, DBG_CLOCKS)) {
            return false;
        }
        if (run->chip->uo_out & PIN_DBG_DO) {
            words[i / 32] |= (uint32_t) 1 << (i % 32);
        }
    }
    run->dbg_di = false;

    // The order of the bits is the one of the comment of `asc_soc.sv`
    const uint32_t expected[] = {
        cpu->r0, cpu->r1, cpu->r2, cpu->r3, cpu->r4, cpu->r5, cpu->r6, cpu->r7, cpu->fp, cpu->ksp, cpu->lr, cpu->usp,
        cpu->saved_ip, cpu->fault_addr, cpu->pt_root, cpu->saved_flags
    };
    static const char *const NAMES[] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "fp", "ksp", "lr", "usp", "cr1", "cr2", "cr3", "cr4"
    };

    bool is_locked = run->chip->cpu_dbg_locked;
    bool is_equal = true;
    uint32_t last = words[DBG_BITS / 32];
    uint32_t shown_cr0_high = (last >> (DBG_CR0_HIGH % 32)) & 0x3;

    if (((last >> (DBG_LOCKED % 32)) & 0x1) != is_locked) {
        fprintf(stderr, "The debug port shows that it is %s, and the CPU has %s it (instruction %zu)\n",
                is_locked ? "open" : "locked", is_locked ? "locked" : "not locked", order);
        is_equal = false;
    }
    if (is_locked) {
        for (size_t i = 0; i < DBG_BITS / 32; ++i) {
            if (words[i] != 0) {
                fprintf(stderr, "The locked debug port shows 0x%08x in its word %zu (instruction %zu)\n", words[i], i, order);
                is_equal = false;
            }
        }
        if (shown_cr0_high != 0) {
            fprintf(stderr, "The locked debug port shows bits of cr0 (instruction %zu)\n", order);
            is_equal = false;
        }
        ++run->debug_locked_reads;
    } else {
        if (shown_cr0_high != ((cpu->ctrl >> 4) & 0x3)) {
            fprintf(stderr, "The debug port shows 0x%x for the bits A and C of cr0, and the CPU has cr0 0x%02x (instruction %zu)\n",
                    shown_cr0_high, cpu->ctrl, order);
            is_equal = false;
        }
        if (words[0] != cpu->ip) {
            fprintf(stderr, "The debug port shows ip 0x%08x and the CPU has 0x%08x (instruction %zu)\n", words[0], cpu->ip, order);
            is_equal = false;
        }
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            if (words[4 + i] != expected[i]) {
                fprintf(stderr, "The debug port shows %s 0x%08x and the CPU has 0x%08x (instruction %zu)\n",
                        NAMES[i], words[4 + i], expected[i], order);
                is_equal = false;
            }
        }
    }
    if (!is_equal) {
        run->status = EXIT_FAILURE;
    }
    ++run->debug_reads;

    return is_equal;
}

/* What a program meets besides memory */
typedef struct trace_world
{
    trace_event_t *events; // bytes of the keyboard by retirement order
    size_t events_count;
    uint8_t *con_in;       // bytes that the console receives
    size_t con_in_size;
    FILE *con_out_file;    // file for the bytes that the program prints, or NULL
} trace_world_t;

/*
    The loop of `asc_reftrace`, with the chip in the place of the CPU, the
    interrupt controller, the timer, and the keyboard controller of the
    emulator.
*/
static int chip_trace(chip_run_t *run, trace_world_t *world)
{
    const trace_args_t *args = run->args;
    device_t *dev = run->dev;

    size_t order = 0;
    size_t wait_steps = 0;
    size_t events_pos = 0;
    size_t con_in_pos = 0;

    if (!chip_reset(run)) {
        return run->status;
    }

    while (order < args->max_retired) {
        bool has_scancode = false;
        uint8_t scancode = 0x00;
        cpu_t cpu;

        // The console gets its input as fast as the program takes it
        while (con_in_pos < world->con_in_size && rp_put_con_input(dev, world->con_in[con_in_pos])) {
            ++con_in_pos;
        }

        if (!chip_step(run)) {
            return run->status;
        }
        chip_read_cpu(run, &cpu);

        if (run->has_retired) {
            wait_steps = 0;
            if (!args->is_quiet) {
                trace_print_row(order, run->fetch_ip, &cpu);
            }
            if (args->debug_period != 0 && order % args->debug_period == 0 && !chip_check_debug(run, &cpu, order)) {
                return run->status;
            }

            if (events_pos < world->events_count && world->events[events_pos].order == order) {
                if (world->events[events_pos].is_button) {
                    rp_set_button(dev, world->events[events_pos].is_down);
                } else {
                    has_scancode = true;
                    scancode = world->events[events_pos].scancode;
                }
                ++events_pos;
            }

            ++order;
        } else if (cpu.halted && ++wait_steps > TRACE_MAX_WAIT_STEPS) {
            fputs("The CPU is halted and no interrupt wakes it\n", stderr);
            return TRACE_EXIT_NO_END;
        }

        uint8_t con_out_byte;
        if (rp_take_con_output(dev, &con_out_byte) && world->con_out_file != NULL) {
            fputc(con_out_byte, world->con_out_file);
        }

        if (cpu.double_fault) {
            trace_report_double_fault(&cpu);
            return TRACE_EXIT_DOUBLE_FAULT;
        }

        if (cpu_is_parked(&cpu)) {
            return trace_report_parked(&cpu);
        }

        /*
            The devices move, in the order of the emulator. The interrupt
            line takes the level that the devices of the RP2040 ask for at
            this moment and keeps it until the next step: the emulator looks
            at the line once per step, here.
        */
        if (!chip_tick(run)) {
            return run->status;
        }
        if (has_scancode && !chip_send_scancode(run, scancode)) {
            return run->status;
        }
        rp_step(dev);
        run->irq = rp_get_irq_line(&dev->rp);
        if (!chip_clocks(run, SETTLE_CLOCKS)) {
            return run->status;
        }
    }

    fputs("The retirement limit was reached before the program parked the CPU\n", stderr);

    return TRACE_EXIT_NO_END;
}

/*
    Lets the chip run on its own time. The devices of the RP2040 move once
    for each instruction that the CPU finishes, as in the emulator, and once
    in `HALT_TURN_CLOCKS` clocks while the CPU is halted. The timer, the
    interrupt controller, and the keyboard controller are in the chip and
    follow its clock.
*/
static int chip_run_free(chip_run_t *run, trace_world_t *world)
{
    const trace_args_t *args = run->args;
    device_t *dev = run->dev;
    Vchip *chip = run->chip;

    size_t order = 0;
    size_t halted_clocks = 0;
    size_t wait_turns = 0;
    size_t events_pos = 0;
    size_t con_in_pos = 0;

    if (!chip_reset(run)) {
        return run->status;
    }
    run->stop = false;

    while (order < args->max_retired) {
        bool is_turn = false;

        run->has_retired = false;
        chip_step_keyboard(run);
        if (!chip_clock(run)) {
            return run->status;
        }

        if (run->has_retired) {
            is_turn = true;
            halted_clocks = 0;
            wait_turns = 0;
            ++order;
        } else if (chip->cpu_boundary && chip->cpu_halted && ++halted_clocks == HALT_TURN_CLOCKS) {
            is_turn = true;
            halted_clocks = 0;
            if (++wait_turns > TRACE_MAX_WAIT_STEPS) {
                fputs("The CPU is halted and no interrupt wakes it\n", stderr);
                return TRACE_EXIT_NO_END;
            }
        }

        // A byte of the keyboard that is due waits until the last one has left the wires
        if (events_pos < world->events_count && world->events[events_pos].order < order && run->kbd_left == 0) {
            if (world->events[events_pos].is_button) {
                rp_set_button(dev, world->events[events_pos].is_down);
            } else {
                chip_start_scancode(run, world->events[events_pos].scancode);
            }
            ++events_pos;
        }

        if (is_turn) {
            while (con_in_pos < world->con_in_size && rp_put_con_input(dev, world->con_in[con_in_pos])) {
                ++con_in_pos;
            }

            uint8_t con_out_byte;
            if (rp_take_con_output(dev, &con_out_byte) && world->con_out_file != NULL) {
                fputc(con_out_byte, world->con_out_file);
            }

            rp_step(dev);
            run->irq = rp_get_irq_line(&dev->rp);
        }

        if (chip->cpu_boundary && (chip->cpu_double_fault || chip->cpu_halted)) {
            cpu_t cpu;
            chip_read_cpu(run, &cpu);

            if (cpu.double_fault) {
                trace_report_double_fault(&cpu);
                return TRACE_EXIT_DOUBLE_FAULT;
            }

            if (cpu_is_parked(&cpu)) {
                return trace_report_parked(&cpu);
            }
        }
    }

    fputs("The retirement limit was reached before the program parked the CPU\n", stderr);

    return TRACE_EXIT_NO_END;
}

static int chip_run(device_t *dev, FILE *bus_file, const trace_args_t *args, trace_world_t *world)
{
    // A flop that the reset does not set starts with a random bit, as on the real chip
    VerilatedContext context;
    const char *seed = getenv("ASC_SEED");
    context.randReset(2);
    context.randSeed(seed != NULL ? (int) strtol(seed, NULL, 10) : RAND_SEED);

    Vchip chip(&context);

    chip_run_t run;
    run.chip = &chip;
    rp_bus_init(&run.bus);
    run.dev = dev;
    run.bus_file = bus_file;
    run.args = args;
    run.stop = true;
    run.irq = false;
    run.kbd_clk = true;
    run.kbd_data = true;
    run.tick = false;
    run.kbd_frame = 0;
    run.kbd_left = 0;
    run.clocks = 0;
    run.status = TRACE_EXIT_ERROR;
    run.has_left = false;
    run.has_retired = false;
    run.fetch_ip = 0;
    run.dbg_ck = false;
    run.dbg_di = false;
    run.debug_reads = 0;
    run.debug_locked_reads = 0;

    int status = args->has_own_time ? chip_run_free(&run, world) : chip_trace(&run, world);

    chip.final();

    // For measurements: the clocks that the run took
    if (getenv("ASC_CLOCKS") != NULL) {
        fprintf(stderr, "The run took %zu clocks\n", run.clocks);
    }

    if (args->debug_period != 0) {
        fprintf(stderr, "The debug port was read and compared %zu times\n", run.debug_reads);
        if (run.debug_locked_reads != 0) {
            fprintf(stderr, "The debug port was locked at %zu of them\n", run.debug_locked_reads);
        }
    }

    // The chip repeats a transaction once for each answer that was held back, and never else
    if (args->fault_period != 0) {
        fprintf(stderr, "The RP2040 has sent %zu answers a second time\n", run.bus.repeat_cnt);
    }
    if (run.bus.repeat_cnt != run.bus.fault_cnt) {
        fprintf(
            stderr,
            "The RP2040 has held back %zu answers and has sent %zu a second time\n",
            run.bus.fault_cnt, run.bus.repeat_cnt
        );
        return TRACE_EXIT_ERROR;
    }

    return status;
}

int main(int argc, char *argv[])
{
    int status = TRACE_EXIT_ERROR;
    trace_args_t args;
    trace_world_t world;
    FILE *bus_file = NULL;
    uint8_t *disk = NULL;
    size_t disk_size = 0;
    firmware_t frm;
    static device_t dev;

    world.events = NULL;
    world.events_count = 0;
    world.con_in = NULL;
    world.con_in_size = 0;
    world.con_out_file = NULL;

    frm.image = NULL;
    frm.data_sec = NULL;
    frm.text_sec = NULL;

    if (parse_args(argc, argv, &args) != 0) {
        fprintf(
            stderr,
            "Usage:\n"
                "\t%s [-n max_retired] [-c max_clocks] [-q] [-r] [-t] [-w answer_delay] [-f fault_period] [-g debug_period] [-e event_file] [-i console_input_file] [-o console_output_file] [-d disk_file] [-s disk_output_file] [-b bus_log_file] <path to the compiled binary>\n",
            PROGRAM_NAME
        );
        goto end;
    }

    if (!firmware_load(args.in_file_path, &frm)) {
        goto end;
    }

    if (trace_read_events(args.event_file_path, &world.events, &world.events_count) != 0) {
        goto end;
    }

    if (args.con_in_file_path != NULL &&
        (world.con_in = trace_read_file(args.con_in_file_path, &world.con_in_size)) == NULL) {
        goto end;
    }

    if (args.con_out_file_path != NULL && (world.con_out_file = fopen(args.con_out_file_path, "wb")) == NULL) {
        perror("Failed to create the console output file");
        goto end;
    }

    if (args.disk_file_path != NULL && (disk = trace_read_file(args.disk_file_path, &disk_size)) == NULL) {
        goto end;
    }

    if (args.bus_file_path != NULL) {
        if ((bus_file = fopen(args.bus_file_path, "w")) == NULL) {
            perror("Failed to create the bus log file");
            goto end;
        }
        fputs(TRACE_BUS_CSV_HEADER "\n", bus_file);
    }

    dev_init(&dev, &frm);
    rp_attach_disk(&dev, disk, disk_size);
    if (args.is_ram_dirty) {
        trace_dirty_ram(&dev, &frm);
    }

    if (!args.is_quiet && !args.has_own_time) {
        puts(TRACE_CSV_HEADER);
    }

    status = chip_run(&dev, bus_file, &args, &world);

end:
    // The disk is saved however the run has ended: what a failed run has left on it helps to find the reason
    if (args.disk_out_file_path != NULL && disk != NULL &&
        trace_write_file(args.disk_out_file_path, disk, disk_size) != 0) {
        status = TRACE_EXIT_ERROR;
    }

    firmware_free(&frm);

    if (world.events != NULL) {
        free(world.events);
    }

    if (world.con_in != NULL) {
        free(world.con_in);
    }

    if (world.con_out_file != NULL) {
        fclose(world.con_out_file);
    }

    if (bus_file != NULL) {
        fclose(bus_file);
    }

    if (disk != NULL) {
        free(disk);
    }

    return status;
}
