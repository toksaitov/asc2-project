#ifndef TRACE_COMMON_H
#define TRACE_COMMON_H

#include <errno.h>
#include <stdlib.h>

#include "../../asc2-emulated/asc.h"

#define TRACE_DEFAULT_MAX_RETIRED 100000
#define TRACE_CSV_HEADER \
    "order,ip,inst,r0,r1,r2,r3,r4,r5,r6,r7,fp,ksp,usp,lr,next_ip,flags,cr0,cr1,cr2,cr3,cr4,cr5,halted"

/*
    One row of the bus log for every request that the CPU sends:

    * `kind`: `fetch` (an instruction fetch with its address), `next` (a
      fetch of "the next word", which carries no address), `read`, or `write`;
    * `mode`: `k` if the CPU was in kernel mode, `u` in user mode;
    * `size`: the number of bytes;
    * `addr`: the physical address; for `next`, the address that the RP2040
      has read;
    * `data`: the value written or read, zero for a read that has failed;
    * `stat`: the answer of the bus (0 if the access was done).
*/
#define TRACE_BUS_CSV_HEADER "kind,mode,size,addr,data,stat"

/*
    `hlt` waits for interrupts, so a trace ends only when the CPU is parked: a
    `hlt` has retired and no interrupt can wake the CPU. A halted CPU retires
    nothing; the wait bound covers the largest PIT period (65536) with margin
    before a trace tool reports a CPU that never wakes.
*/
#define TRACE_MAX_WAIT_STEPS 70000

/*
    A test program reports its own result in `r0` when it parks the CPU: the
    pass code if every check has passed, and the number of the first failed
    check otherwise. The pass code is not zero because empty memory is full of
    zeros, and a zero is the instruction `hlt`: a program that jumps into
    empty memory by mistake parks the CPU too, most likely with a zero or a
    small number in `r0`.
*/
#define TRACE_PASS_CODE 0x600d

/* Exit statuses of a trace tool. */
#define TRACE_EXIT_PASSED       0 // the program parked the CPU with the pass code in `r0`
#define TRACE_EXIT_FAILED       1 // the program parked the CPU with a failure code in `r0`
#define TRACE_EXIT_ERROR        2 // the tool could not run the program
#define TRACE_EXIT_DOUBLE_FAULT 3 // the CPU stopped with a double fault
#define TRACE_EXIT_NO_END       4 // the program did not park the CPU within the limits

#define TRACE_DIRTY_RAM_BYTE 0xff // byte that fills RAM on request: four of them are no instruction and no empty page table entry

/*
    An event arrives after the instruction with the number `order` has
    retired. A line of an event file is `order,byte` for a byte that the
    keyboard sends, or `order,down` and `order,up` for the push button.
*/
typedef struct trace_event
{
    size_t order;
    uint8_t scancode;
    bool is_button; // the event is a new level of the push button, and no byte of the keyboard
    bool is_down;   // the level: the button is down
} trace_event_t;

/*
    Fills the RAM that the firmware image does not set with
    `TRACE_DIRTY_RAM_BYTE`. The RAM of the real machine keeps its bytes over a
    reset, and only the data section of the image is set anew. A program that
    counts on zeros elsewhere works in the emulator and fails on the machine,
    so a test can ask for RAM that is full of other bytes.
*/
static void trace_dirty_ram(device_t *dev, firmware_t *firmware)
{
    assert(dev != NULL);
    assert(firmware != NULL);

    size_t data_pos = FIRMWARE_DATA_ADDR - KERN_BASE - RAM_BASE;
    size_t data_end = data_pos + firmware->data_sec_size;

    memset(dev->mem.ram, TRACE_DIRTY_RAM_BYTE, data_pos);
    memset(dev->mem.ram + data_end, TRACE_DIRTY_RAM_BYTE, RAM_SIZE - data_end);
}

/*
    Prints the bus requests from the index `*printed` on and moves the index
    past them.
*/
static void trace_print_bus_rows(FILE *file, device_t *dev, size_t *printed)
{
    assert(file != NULL);
    assert(dev != NULL);
    assert(printed != NULL);

    for (; *printed < dev->bus_reqs_cnt; ++*printed) {
        bus_req_t *req = &dev->bus_reqs[*printed];

        const char *kind = "read";
        if (req->next) {
            kind = "next";
        } else if (req->access == MEM_FETCH) {
            kind = "fetch";
        } else if (req->access == MEM_WRITE) {
            kind = "write";
        }

        fprintf(
            file,
            "%s,%c,%u,%08x,%08x,%u\n",
            kind,
            req->kernel ? 'k' : 'u',
            (unsigned) req->size,
            req->addr,
            req->data,
            (unsigned) req->stat
        );
    }
}

static int trace_cmp_events(const void *lhs, const void *rhs)
{
    const trace_event_t *event_lhs = (const trace_event_t *) lhs;
    const trace_event_t *event_rhs = (const trace_event_t *) rhs;

    if (event_lhs->order < event_rhs->order) {
        return -1;
    }

    if (event_lhs->order > event_rhs->order) {
        return 1;
    }

    return 0;
}

static int trace_parse_event_line(
    const char *line,
    size_t line_num,
    trace_event_t *event,
    int *parsed
)
{
    assert(line != NULL);
    assert(event != NULL);
    assert(parsed != NULL);

    const char *cursor = line;
    char *endptr = NULL;
    unsigned long long order = 0;
    unsigned long scancode = 0;

    *parsed = 0;

    while (isspace((unsigned char) *cursor)) {
        ++cursor;
    }

    if (*cursor == '\0' || *cursor == '\n' || *cursor == '#') {
        return 0;
    }

    errno = 0;
    order = strtoull(cursor, &endptr, 0);
    if (errno != 0 || endptr == cursor) {
        fprintf(stderr, "Invalid event order on line %zu\n", line_num);
        return -1;
    }
    cursor = endptr;

    while (isspace((unsigned char) *cursor)) {
        ++cursor;
    }

    if (*cursor != ',') {
        fprintf(stderr, "Missing comma in the event file on line %zu\n", line_num);
        return -1;
    }
    ++cursor;

    while (isspace((unsigned char) *cursor)) {
        ++cursor;
    }

    bool is_button = false;
    bool is_down = false;
    if (strncmp(cursor, "down", 4) == 0) {
        is_button = true;
        is_down = true;
        cursor += 4;
    } else if (strncmp(cursor, "up", 2) == 0) {
        is_button = true;
        cursor += 2;
    } else {
        errno = 0;
        scancode = strtoul(cursor, &endptr, 0);
        if (errno != 0 || endptr == cursor || scancode > UINT8_MAX) {
            fprintf(stderr, "Invalid scancode in the event file on line %zu\n", line_num);
            return -1;
        }
        cursor = endptr;
    }

    while (isspace((unsigned char) *cursor)) {
        ++cursor;
    }

    if (*cursor != '\0' && *cursor != '\n' && *cursor != '#') {
        fprintf(stderr, "Invalid event file contents on line %zu\n", line_num);
        return -1;
    }

    event->order = (size_t) order;
    event->scancode = (uint8_t) scancode;
    event->is_button = is_button;
    event->is_down = is_down;
    *parsed = 1;

    return 0;
}

static int trace_read_events(
    const char *event_file_path,
    trace_event_t **events,
    size_t *events_count
)
{
    assert(events != NULL);
    assert(events_count != NULL);

    int status = -1;
    FILE *event_file = NULL;
    trace_event_t *parsed_events = NULL;
    size_t parsed_events_count = 0;
    size_t parsed_events_cap = 0;
    char line[128];
    size_t line_num = 0;

    *events = NULL;
    *events_count = 0;

    if (event_file_path == NULL) {
        return 0;
    }

    event_file = fopen(event_file_path, "r");
    if (event_file == NULL) {
        perror("Failed to open the event file");
        goto end;
    }

    while (fgets(line, sizeof(line), event_file) != NULL) {
        int parsed = 0;
        trace_event_t event;

        ++line_num;

        if (trace_parse_event_line(line, line_num, &event, &parsed) != 0) {
            goto end;
        }

        if (!parsed) {
            continue;
        }

        if (parsed_events_count == parsed_events_cap) {
            size_t new_cap = parsed_events_cap == 0 ? 8 : parsed_events_cap * 2;
            trace_event_t *new_events = (trace_event_t *) realloc(parsed_events, new_cap * sizeof(*parsed_events));

            if (new_events == NULL) {
                perror("Not enough memory to store event rows");
                goto end;
            }

            parsed_events = new_events;
            parsed_events_cap = new_cap;
        }

        parsed_events[parsed_events_count++] = event;
    }

    if (ferror(event_file)) {
        perror("Failed to read the event file");
        goto end;
    }

    qsort(parsed_events, parsed_events_count, sizeof(*parsed_events), trace_cmp_events);

    for (size_t i = 1; i < parsed_events_count; ++i) {
        if (parsed_events[i - 1].order == parsed_events[i].order) {
            fprintf(stderr, "Duplicate event order %zu in the event file\n", parsed_events[i].order);
            goto end;
        }
    }

    *events = parsed_events;
    *events_count = parsed_events_count;
    parsed_events = NULL;
    status = 0;

end:
    if (parsed_events != NULL) {
        free(parsed_events);
    }

    if (event_file != NULL) {
        fclose(event_file);
    }

    return status;
}

/*
    Writes what the screen shows as text: a line with the number of frames
    that have reached the screen, then a line for every row of the screen
    with `#` for a pixel that is on and `.` for one that is off.
*/
static int trace_write_screen(const char *path, rp_t *rp)
{
    assert(path != NULL);
    assert(rp != NULL);

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror("Failed to create the screen file");
        return -1;
    }

    fprintf(file, "frames %u\n", (unsigned) rp->scr_frames);
    for (size_t y = 0; y < RP_SCR_HEIGHT; ++y) {
        for (size_t x = 0; x < RP_SCR_WIDTH; ++x) {
            bool is_on = (rp->scr_shown[y * (RP_SCR_WIDTH / 8) + x / 8] >> (x % 8)) & 1;
            fputc(is_on ? '#' : '.', file);
        }
        fputc('\n', file);
    }

    if (fclose(file) != 0) {
        perror("Failed to write the screen file");
        return -1;
    }

    return 0;
}

/*
    Reads a whole file into memory. An empty file gives a buffer of one byte so
    that the result is never NULL on success.
*/
static uint8_t *trace_read_file(const char *path, size_t *size)
{
    assert(path != NULL);
    assert(size != NULL);

    uint8_t *bytes = NULL;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return NULL;
    }

    long file_size = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        file_size = ftell(file);
    }
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        perror(path);
        goto end;
    }

    bytes = (uint8_t *) malloc(file_size > 0 ? (size_t) file_size : 1);
    if (bytes == NULL) {
        perror("Not enough memory to read a file");
        goto end;
    }

    if (file_size > 0 && fread(bytes, (size_t) file_size, 1, file) != 1) {
        perror(path);
        free(bytes);
        bytes = NULL;
        goto end;
    }

    *size = (size_t) file_size;

end:
    fclose(file);

    return bytes;
}

/*
    Writes bytes into a file that is new or loses what it had. Returns 0, or
    -1 with a message.
*/
static int trace_write_file(const char *path, const uint8_t *bytes, size_t size)
{
    assert(path != NULL);
    assert(bytes != NULL);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return -1;
    }

    bool is_written = size == 0 || fwrite(bytes, size, 1, file) == 1;
    if (fclose(file) != 0) {
        is_written = false;
    }
    if (!is_written) {
        perror(path);
        return -1;
    }

    return 0;
}

/*
    Prints one row of the trace: the state of the CPU after the instruction
    at `ret_ip` has retired. The columns are the count of instructions so
    far, `ret_ip`, the instruction, `r0` to `r7`, `fp`, the two stack
    pointers (kernel, then user), `lr`, `ip`, `flags`, `cr0` to `cr5`, and
    1 if the CPU is halted.
*/
static void trace_print_row(size_t order, uint32_t ret_ip, cpu_t *cpu)
{
    assert(cpu != NULL);

    printf(
        "%zu,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%02x,%x,%08x,%08x,%08x,%02x,%08x,%d\n",
        order,
        ret_ip,
        cpu->inst,
        cpu->r0,
        cpu->r1,
        cpu->r2,
        cpu->r3,
        cpu->r4,
        cpu->r5,
        cpu->r6,
        cpu->r7,
        cpu->fp,
        cpu->ksp,
        cpu->usp,
        cpu->lr,
        cpu->ip,
        cpu->flags,
        cpu->ctrl,
        cpu->saved_ip,
        cpu->fault_addr,
        cpu->pt_root,
        cpu->saved_flags,
        cpu->cause,
        cpu->halted
    );
}

/*
    Tells why a CPU that has stopped with a double fault did so. The trap
    record is the one of the first trap: a double fault keeps it.
*/
static void trace_report_double_fault(cpu_t *cpu)
{
    assert(cpu != NULL);

    fprintf(
        stderr,
        "The CPU stopped with a double fault at ip 0x%08x: cr1 0x%08x, cr2 0x%08x, cr4 0x%02x, cr5 0x%08x\n",
        cpu->ip, cpu->saved_ip, cpu->fault_addr, cpu->saved_flags, cpu->cause
    );
}

/*
    Reads the result of a program that has parked the CPU and returns the
    exit status that goes with it.
*/
static int trace_report_parked(cpu_t *cpu)
{
    assert(cpu != NULL);

    if (cpu->r0 != TRACE_PASS_CODE) {
        fprintf(
            stderr,
            "The program ended at ip 0x%08x with the failure code %u (0x%x) in r0\n",
            cpu->ip, cpu->r0, cpu->r0
        );
        return TRACE_EXIT_FAILED;
    }

    return TRACE_EXIT_PASSED;
}

#endif
