// Compares the memory and the devices of the bridge (`devices.h`) with the
// ones of the emulator (`asc.h`) on random requests.
//
// Both get the same RAM, boot ROM, and disk. Every request goes to both, and
// the test compares the answers, the values read, the interrupt line, and
// the console. The test also does the work of core 0: it types bytes, takes
// the printed ones, presses the button, and ends a disk command or the way
// of a frame to the screen when the emulator ends its own.
// Now and then both are reset. At the end RAM, the disk, and the frame
// memory must be equal byte for byte.

#include "../../../asc2-emulated/asc.h"

#include "../devices.h"

// The two constants of the bridge that have other names than the emulator's
_Static_assert(BOOT_ROM_BASE == ROM_BASE, "the boot ROM is at different addresses");
_Static_assert(BOOT_ROM_SIZE == ROM_SIZE, "the boot ROM has different sizes");
_Static_assert(CON_IN_SIZE == RP_CON_BUFF_SIZE, "the console input queues have different sizes");

#define REQUESTS   4000000
#define RESETS     16     // resets of the devices in the course of the requests
#define ROM_LEN    0x3000 // bytes of the boot ROM's image; the rest of the range is zeros
#define DISK_SECS  8
#define SEED       0x2545f491

#define PICTURES_ADDR 0x1000 // pictures for the screen, in a part of RAM that the random writes reach seldom
#define PICTURES      16
#define PICTURE_STEP  0x420  // room for the largest picture: a word, then 64 rows of 4 words

static device_t dev;
static bridge_t bridge;

static uint8_t bridge_ram[RAM_SIZE];
static uint8_t bridge_high[HIGH_SIZE];
static uint8_t bridge_rom[ROM_LEN];

static uint8_t dev_disk[DISK_SECS * RP_DISK_SEC_SIZE];
static uint8_t bridge_disk[DISK_SECS * RP_DISK_SEC_SIZE];

static uint32_t rnd_state = SEED;
static unsigned long long request_cnt;

static bool is_typing;                    // bytes arrive at the console now
static unsigned long long no_data_cnt;    // reads of the console that found no byte
static unsigned long long full_cnt;       // typed bytes that found no room
static unsigned long long disk_cnt;       // disk commands that ended
static unsigned long long disk_error_cnt; // the ones that ended with an error
static unsigned long long frame_cnt;      // frames that reached the screen
static unsigned long long press_cnt;      // presses of the button
static unsigned long long user_cnt;       // accesses of a user program to the page of the screen
static unsigned long long console_cnt;    // bytes that the console has drawn on the screen

static uint32_t rnd(void)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;

    return rnd_state;
}

static void fail(const char *what, uint8_t hdr, uint32_t addr)
{
    fprintf(stderr, "FAILED: devices_test, request %llu (header 0x%02x, address 0x%06x): %s\n",
            request_cnt, (unsigned) hdr, (unsigned) addr, what);
    exit(EXIT_FAILURE);
}

// Addresses that meet every range and every edge of a range
static uint32_t rnd_addr(void)
{
    static const uint32_t REGS[] = {
        RP_ID_ADDR,       RP_SCRATCH_ADDR,  RP_RAM_SIZE_ADDR, RP_TIME_ADDR,       RP_CON_STAT_ADDR,
        RP_CON_DATA_ADDR, RP_IRQ_STAT_ADDR, RP_IRQ_STAT_ADDR, RP_IRQ_ENABLE_ADDR, RP_DISK_SEC_ADDR,
        RP_DISK_BUF_ADDR, RP_DISK_CMD_ADDR, RP_DISK_STAT_ADDR, RP_LOSE_ADDR,      RP_BTN_STAT_ADDR,
        RP_BTN_COUNT_ADDR, RP_SCR_STAT_ADDR, RP_SCR_CMD_ADDR, RP_SCR_CMD_ADDR,    RP_SCR_FRAMES_ADDR,
        RP_SCR_CHAR_ADDR, RP_SCR_CHAR_ADDR,  RP_SCR_RECT_ADDR,  RP_SCR_RECT_ADDR, RP_SCR_SRC_ADDR,
        RP_SCR_SRC_ADDR,  RP_SCR_BLIT_ADDR,  RP_SCR_BLIT_ADDR,  RP_SCR_BLIT_ADDR, RP_SCR_LINE_ADDR + 4,
        RP_SCR_PIC_ADDR,  RP_SCR_PIC_ADDR + 4, RP_SCR_PIC_ADDR + 8, RP_SCR_PIC_ADDR + 12, RP_SCR_PIC_ADDR + 16,
        RP_SCR_PIC_ADDR + 20, RP_SCR_PIC_ADDR + 24, RP_SCR_PIC_ADDR + 28, RP_CON_DATA_ADDR, RP_CON_DATA_ADDR,
        RP_USER_SCR_STAT_ADDR, RP_USER_SCR_CMD_ADDR, RP_USER_SCR_FRAMES_ADDR, RP_USER_SCR_CHAR_ADDR, RP_USER_SCR_RECT_ADDR,
        RP_USER_SCR_BLIT_ADDR, RP_USER_SCR_BLIT_ADDR, RP_USER_SCR_BLIT_ADDR, RP_USER_BTN_STAT_ADDR, RP_USER_BTN_COUNT_ADDR,
        RP_USER_TIME_ADDR, RP_USER_SCR_LINE_ADDR + 4, RP_SCR_MOVE_ADDR, RP_SCR_LINE_ADDR, RP_SCR_LINE_ADDR,
        RP_USER_SCR_MOVE_ADDR, RP_USER_SCR_LINE_ADDR, RP_USER_SCR_LINE_ADDR
    };
    static const uint32_t EDGES[] = {
        0, RAM_SIZE, ROM_BASE, ROM_BASE + ROM_LEN, ROM_BASE + ROM_SIZE, RP_BASE, RP_BASE + RP_SIZE, 0x80000000, HIGH_BASE,
        HIGH_BASE + HIGH_SIZE, RP_SCR_BASE, RP_SCR_BASE + RP_SCR_SIZE
    };

    uint32_t kind = rnd() % 20;
    if (kind >= 18) {
        return RP_SCR_BASE + (rnd() % RP_SCR_SIZE); // the frame memory
    } else if (kind >= 17) {
        return HIGH_BASE + (rnd() % HIGH_SIZE);
    } else if (kind >= 16) {
        return rnd() % 0x400 + ((rnd() % 256) << 24); // RAM, the boot ROM, or a register with other upper bits
    } else if (kind < 4) {
        return rnd() % 0x400; // a small part of RAM, so that reads meet earlier writes
    } else if (kind < 5) {
        return rnd() % 2 == 0 ? RP_CON_DATA_ADDR : RP_CON_STAT_ADDR;
    } else if (kind < 7) {
        return rnd() % RAM_SIZE;
    } else if (kind < 9) {
        return ROM_BASE + (rnd() % (ROM_LEN + 0x100));
    } else if (kind < 13) {
        return REGS[rnd() % (sizeof(REGS) / sizeof(REGS[0]))] + (rnd() % 16 == 0 ? rnd() % 4 : 0);
    } else if (kind < 15) {
        return EDGES[rnd() % (sizeof(EDGES) / sizeof(EDGES[0]))] + (rnd() % 16) - 8;
    } else {
        return rnd();
    }
}

// A value that often makes sense in a register of the disk or of the test register
static uint32_t rnd_data(uint32_t addr)
{
    uint32_t kind = rnd() % 8;

    // A register of the user page takes what its twin takes
    if (addr == RP_USER_SCR_CMD_ADDR) {
        addr = RP_SCR_CMD_ADDR;
    } else if (addr == RP_USER_SCR_CHAR_ADDR) {
        addr = RP_SCR_CHAR_ADDR;
    } else if (addr == RP_USER_SCR_RECT_ADDR) {
        addr = RP_SCR_RECT_ADDR;
    } else if (addr == RP_USER_SCR_BLIT_ADDR) {
        addr = RP_SCR_BLIT_ADDR;
    } else if (addr == RP_USER_SCR_MOVE_ADDR) {
        addr = RP_SCR_MOVE_ADDR;
    } else if (addr == RP_USER_SCR_LINE_ADDR) {
        addr = RP_SCR_LINE_ADDR;
    }

    if (kind == 0) {
        return rnd();
    } else if (addr == RP_CON_DATA_ADDR) {
        // Text most of the time, with the bytes that move the cursor and the start of an escape sequence
        static const uint8_t MOVES[] = { '\n', '\n', '\r', '\b', 0x7f, 0x1b, '[', 'm' };
        return kind < 6 ? 0x20 + rnd() % 0x5f : MOVES[rnd() % sizeof(MOVES)];
    } else if (addr >= RP_SCR_PIC_ADDR && addr < RP_SCR_PIC_ADDR + RP_SCR_PICS * 8) {
        if ((addr & 4) == 0) {
            // The start of a picture: one that the test keeps, the last words of RAM, or anything
            if (kind < 6) {
                return PICTURES_ADDR + (rnd() % PICTURES) * PICTURE_STEP;
            }
            return kind == 6 ? RAM_SIZE - 4 * (rnd() % 8) : rnd() % RAM_SIZE;
        }
        // The page where it goes on: none, a page of RAM, the last one, or anything
        return kind < 3 ? 0 : kind < 6 ? (rnd() % (RAM_SIZE / PAGE_SIZE)) * PAGE_SIZE : kind == 6 ? RAM_SIZE - PAGE_SIZE + (rnd() % 2) * PAGE_SIZE : rnd() % RAM_SIZE;
    } else if (addr == RP_DISK_SEC_ADDR) {
        return rnd() % (DISK_SECS + 2);
    } else if (addr == RP_DISK_BUF_ADDR) {
        if (kind == 1) {
            return RAM_SIZE - RP_DISK_SEC_SIZE + (rnd() % 3) * 4 - 4; // around the last place that holds a sector
        } else if (kind == 2) {
            return rnd() % (RAM_SIZE - RP_DISK_SEC_SIZE);             // often not a multiple of 4
        }
        return (rnd() % (RAM_SIZE / 4)) * 4;
    } else if (addr == RP_DISK_CMD_ADDR) {
        return rnd() % 4;
    } else if (addr == RP_LOSE_ADDR) {
        return rnd() % 6;
    } else if (addr == RP_SCR_CHAR_ADDR) {
        // A character at a place that is on the screen most of the time
        return (rnd() % (RP_SCR_ROWS + 2)) << 16 | (rnd() % (RP_SCR_COLS + 2)) << 8 | (rnd() % 0x90);
    } else if (addr == RP_SCR_RECT_ADDR) {
        return rnd();
    } else if (addr == RP_SCR_MOVE_ADDR || addr == RP_SCR_LINE_ADDR) {
        // A point on the screen or near it most of the time, one around the last points that count, or anything
        if (kind < 5) {
            return ((rnd() % 160 - 48) & 0xffff) << 16 | ((rnd() % 224 - 48) & 0xffff);
        } else if (kind < 7) {
            return ((rnd() % 4 + (rnd() % 2 ? 509 : -514)) & 0xffff) << 16 | ((rnd() % 4 + (rnd() % 2 ? 509 : -514)) & 0xffff);
        }
        return rnd();
    } else if (addr == RP_SCR_SRC_ADDR) {
        // One of the pictures that the test keeps in RAM, the last words of RAM, or anything
        if (kind < 6) {
            return PICTURES_ADDR + (rnd() % PICTURES) * PICTURE_STEP;
        }
        return kind == 6 ? RAM_SIZE - 4 * (rnd() % 8) : rnd() % RAM_SIZE;
    } else if (addr == RP_SCR_BLIT_ADDR) {
        // A place near the screen most of the time, and every way to draw
        if (kind < 6) {
            return (rnd() % 16) << 16 | ((rnd() % 100 - 20) & 0xff) << 8 | ((rnd() % 180 - 30) & 0xff);
        }
        return rnd();
    } else if (addr >= RP_SCR_BASE && addr < RP_SCR_BASE + RP_SCR_SIZE) {
        return rnd();
    } else {
        return rnd() % 4;
    }
}

// Writes the first word of every picture anew: sizes that fit the screen
// most of the time, and now and then one that does not.
static void put_pictures(void)
{
    for (uint32_t i = 0; i < PICTURES; ++i) {
        uint32_t width = rnd() % 16 == 0 ? rnd() % 300 : rnd() % RP_SCR_WIDTH + 1;
        uint32_t height = rnd() % 16 == 0 ? rnd() % 100 : rnd() % RP_SCR_HEIGHT + 1;
        uint32_t addr = PICTURES_ADDR + i * PICTURE_STEP;
        for (uint32_t j = 0; j < 4; ++j) {
            dev.mem.ram[addr + j] = bridge_ram[addr + j] = (uint8_t) ((width | height << 16) >> (8 * j));
        }
    }
}

// One step of the emulator's devices, and the same work for the bridge
static void step(void)
{
    bool was_busy = (dev.rp.disk_stat & RP_DISK_FLAG_BUSY) != 0;
    bool was_scr_busy = (dev.rp.scr_stat & RP_SCR_FLAG_BUSY) != 0;
    rp_step(&dev);
    bool is_busy = (dev.rp.disk_stat & RP_DISK_FLAG_BUSY) != 0;
    bool is_scr_busy = (dev.rp.scr_stat & RP_SCR_FLAG_BUSY) != 0;

    if (was_scr_busy != bridge_screen_is_busy(&bridge)) {
        fail("one screen is busy and the other is not", 0, 0);
    }

    // Core 0: the frame that the command has copied goes to the screen
    if (was_scr_busy && !is_scr_busy) {
        if (memcmp(dev.rp.scr_shown, bridge.scr_sent, RP_SCR_SIZE) != 0) {
            fail("the screens show different frames", 0, 0);
        }
        bridge_screen_finish(&bridge);
        ++frame_cnt;
    }

    // Core 0: the button
    if (rnd() % 64 == 0) {
        bool is_down = !dev.rp.btn_down;
        rp_set_button(&dev, is_down);
        bridge_set_button(&bridge, is_down);
        if (is_down) {
            ++press_cnt;
        }
    }

    if (was_busy != bridge_disk_is_busy(&bridge)) {
        fail("one disk is busy and the other is not", 0, 0);
    }

    // Core 0: the work of a disk command
    if (was_busy && !is_busy) {
        bool is_valid = bridge_disk_is_valid(&bridge, DISK_SECS);
        if (is_valid) {
            uint8_t *sec = bridge_disk + (size_t) bridge.disk_sec * RP_DISK_SEC_SIZE;
            uint8_t *buf = bridge_ram + bridge.disk_buf;
            if (bridge.disk_cmd == RP_DISK_CMD_READ) {
                memcpy(buf, sec, RP_DISK_SEC_SIZE);
            } else {
                memcpy(sec, buf, RP_DISK_SEC_SIZE);
            }
        }
        bridge_disk_finish(&bridge, is_valid);

        ++disk_cnt;
        if (!is_valid) {
            ++disk_error_cnt;
        }
    }

    // Core 0: the console. Bytes arrive in long bursts, so that the queue is full at times and empty at others.
    if (rnd() % 50000 == 0) {
        is_typing = !is_typing;
    }
    if (is_typing && rnd() % 2 == 0) {
        uint8_t byte = (uint8_t) rnd();
        bool dev_has_room = rp_put_con_input(&dev, byte);
        if (dev_has_room != bridge_put_con_input(&bridge, byte)) {
            fail("one console input queue is full and the other is not", 0, 0);
        }
        if (!dev_has_room) {
            ++full_cnt;
        }
    }

    if (rp_get_irq_line(&dev.rp) != bridge_get_irq_line(&bridge)) {
        fail("the interrupt lines differ after a step", 0, 0);
    }
}

// Core 0: takes the byte that a request has printed. The emulator holds one
// byte, which its runner takes after every instruction.
static void take_con_output(uint8_t hdr, uint32_t addr)
{
    uint8_t dev_byte = 0;
    uint8_t bridge_byte = 0;
    bool dev_has_byte = rp_take_con_output(&dev, &dev_byte);
    bool bridge_has_byte = bridge_take_con_output(&bridge, &bridge_byte);
    if (dev_has_byte != bridge_has_byte || dev_byte != bridge_byte) {
        fail("the consoles have printed different bytes", hdr, addr);
    }
}

// The requests that the emulator refuses to take, because its CPU never sends them
static void test_refused_requests(void)
{
    uint32_t data = 0;
    bool is_lost = false;

    bridge_reset(&bridge);

    if (bridge_access(&bridge, HDR_KERNEL | (3 << HDR_SIZE_SHIFT), 0x100, 0, &data, &is_lost) != BUS_STAT_ERROR) {
        fail("a request of 8 bytes is not refused", 0, 0);
    }
    if (bridge_access(&bridge, HDR_KERNEL | (2 << HDR_SIZE_SHIFT), 0x102, 0, &data, &is_lost) != BUS_STAT_MISALIGNED ||
        bridge_access(&bridge, HDR_KERNEL | (1 << HDR_SIZE_SHIFT), 0x101, 0, &data, &is_lost) != BUS_STAT_MISALIGNED) {
        fail("a misaligned request is not refused", 0, 0);
    }

    // "The next word" belongs to a fetch: a data read has nothing to continue, even after a fetch
    if (bridge_access(&bridge, HDR_KERNEL | HDR_FETCH | (2 << HDR_SIZE_SHIFT), ROM_BASE, 0, &data, &is_lost) != BUS_STAT_OK ||
        bridge_access(&bridge, HDR_KERNEL | HDR_NEXT | (2 << HDR_SIZE_SHIFT), 0, 0, &data, &is_lost) != BUS_STAT_BAD_NEXT ||
        bridge_access(&bridge, HDR_KERNEL | HDR_NEXT | HDR_WRITE | (2 << HDR_SIZE_SHIFT), 0, 0, &data, &is_lost) != BUS_STAT_BAD_NEXT) {
        fail("a data access to \"the next word\" is not refused", 0, 0);
    }
}

// A console that nobody reads: the queue fills, the status says so, and more bytes are counted as lost
static void test_full_console(void)
{
    uint32_t data = 0;
    bool is_lost = false;
    uint8_t hdr_read = HDR_KERNEL | (2 << HDR_SIZE_SHIFT);
    uint8_t hdr_write = hdr_read | HDR_WRITE;

    for (uint32_t i = 0; i < CON_OUT_SIZE; ++i) {
        bridge_access(&bridge, hdr_read, RP_CON_STAT_ADDR, 0, &data, &is_lost);
        if ((data & RP_CON_FLAG_OUTPUT) == 0) {
            fail("the console refuses a byte that it has room for", 0, 0);
        }
        data = i;
        bridge_access(&bridge, hdr_write, RP_CON_DATA_ADDR, 0, &data, &is_lost);
    }

    bridge_access(&bridge, hdr_read, RP_CON_STAT_ADDR, 0, &data, &is_lost);
    if ((data & RP_CON_FLAG_OUTPUT) != 0) {
        fail("a full console says that it can take a byte", 0, 0);
    }
    data = 0xff;
    bridge_access(&bridge, hdr_write, RP_CON_DATA_ADDR, 0, &data, &is_lost);
    if (bridge.con_out_lost != 1) {
        fail("a byte printed to a full console is not counted as lost", 0, 0);
    }

    for (uint32_t i = 0; i < CON_OUT_SIZE; ++i) {
        uint8_t byte = 0;
        if (!bridge_take_con_output(&bridge, &byte) || byte != (uint8_t) i) {
            fail("the console gives its bytes back in another order", 0, 0);
        }
    }
    uint8_t byte = 0;
    if (bridge_take_con_output(&bridge, &byte)) {
        fail("the console gives back more bytes than it took", 0, 0);
    }
}

int main(void)
{
    mem_init(&dev.mem);
    rp_init(&dev.rp);
    dev.bus_reqs_cnt = 0;

    bridge.ram = bridge_ram;
    bridge.rom = bridge_rom;
    bridge.high = bridge_high;
    bridge.rom_len = ROM_LEN;

    test_refused_requests();
    test_full_console();
    bridge_reset(&bridge);
    if (bridge.con_out_lost != 0) {
        fail("a reset keeps the count of lost bytes", 0, 0);
    }

    for (size_t i = 0; i < RAM_SIZE; ++i) {
        dev.mem.ram[i] = bridge_ram[i] = (uint8_t) rnd();
    }
    for (size_t i = 0; i < HIGH_SIZE; ++i) {
        dev.mem.high[i] = bridge_high[i] = (uint8_t) rnd();
    }
    for (size_t i = 0; i < ROM_LEN; ++i) {
        dev.mem.rom[i] = bridge_rom[i] = (uint8_t) rnd();
    }
    for (size_t i = 0; i < sizeof(dev_disk); ++i) {
        dev_disk[i] = bridge_disk[i] = (uint8_t) rnd();
    }
    rp_attach_disk(&dev, dev_disk, sizeof(dev_disk));

    unsigned long long stat_cnt[8] = { 0 };
    unsigned long long lost_cnt = 0;

    for (request_cnt = 0; request_cnt < REQUESTS; ++request_cnt) {
        // A reset of the chip: the devices start anew, and RAM and the disk keep their bytes
        if (request_cnt % (REQUESTS / RESETS) == REQUESTS / RESETS - 1) {
            rp_init(&dev.rp);
            rp_attach_disk(&dev, dev_disk, sizeof(dev_disk));
            bridge_reset(&bridge);
        }

        if (request_cnt % 4096 == 0) {
            put_pictures();
        }

        // A request as the CPU sends it: a read, a write, or a fetch, with an address that is a multiple of its size.
        // A device register takes 4 bytes in kernel mode, so most requests to one ask so.
        uint32_t kind = rnd() % 4;
        mem_access_t access = kind == 0 ? MEM_WRITE : kind == 1 ? MEM_FETCH : MEM_READ;
        uint32_t addr = rnd_addr();
        bool is_reg = addr >= RP_BASE && addr < RP_BASE + RP_SIZE && rnd() % 8 != 0;
        uint32_t size_code = access == MEM_FETCH || is_reg ? 2 : rnd() % 3; // an instruction has 4 bytes
        uint8_t size = (uint8_t) (1 << size_code);
        bool is_user_page = addr >= RP_USER_BASE && addr < RP_USER_BASE + RP_USER_SIZE;
        bool kernel = is_user_page ? rnd() % 2 == 0 : is_reg || rnd() % 4 != 0; // a user program reaches the page of the screen
        if (is_user_page && !kernel && access != MEM_FETCH) {
            ++user_cnt;
        }
        bool next = access == MEM_FETCH && rnd() % 2 == 0;
        addr &= ~((uint32_t) size - 1);
        uint32_t wdata = rnd_data(addr);

        uint8_t hdr = (uint8_t) (size_code << HDR_SIZE_SHIFT);
        hdr |= access == MEM_WRITE ? HDR_WRITE : 0;
        hdr |= access == MEM_FETCH ? HDR_FETCH : 0;
        hdr |= next ? HDR_NEXT : 0;
        hdr |= kernel ? HDR_KERNEL : 0;
        hdr |= (uint8_t) (rnd() & HDR_SEQ); // the devices do not look at the sequence bit

        dev.bus_reqs_cnt = 0;
        uint32_t dev_data = wdata;
        uint8_t dev_stat = bus_access(&dev, addr, size, access, kernel, next, &dev_data);

        uint32_t bridge_data = wdata;
        bool is_lost = false;
        uint8_t bridge_stat = bridge_access(&bridge, hdr, next ? rnd() : addr, dev.rp.time_us, &bridge_data, &is_lost);

        // The emulator hides the answer of an access whose answer is lost
        if (is_lost) {
            ++lost_cnt;
            if (dev_stat != BUS_STAT_LINK_ERROR) {
                fail("the bridge loses an answer that the emulator gives", hdr, addr);
            }
        } else {
            if (dev_stat != bridge_stat) {
                fail("the answers differ", hdr, addr);
            }
            if (dev_stat == BUS_STAT_OK && access != MEM_WRITE && dev_data != bridge_data) {
                fail("the values read differ", hdr, addr);
            }
            if (dev_stat == BUS_STAT_OK && access == MEM_READ && addr == RP_CON_DATA_ADDR && dev_data == RP_CON_NO_DATA) {
                ++no_data_cnt;
            }
            ++stat_cnt[dev_stat];
        }

        if (dev.rp.fetch_valid != bridge.fetch_valid || (dev.rp.fetch_valid && dev.rp.fetch_addr != bridge.fetch_addr)) {
            fail("the places of \"the next word\" differ", hdr, addr);
        }
        if (rp_get_irq_line(&dev.rp) != bridge_get_irq_line(&bridge)) {
            fail("the interrupt lines differ", hdr, addr);
        }
        take_con_output(hdr, addr);

        // The console draws the same text on both screens, and both give the screen to a program at the same time
        if ((dev.rp.scr_console != 0) != (bridge.scr_console != 0)) {
            fail("the screen is the console's in one and a program's in the other", hdr, addr);
        }
        if (dev.rp.scr_console && access == MEM_WRITE && dev_stat == BUS_STAT_OK && addr == RP_CON_DATA_ADDR && size == 4) {
            if (memcmp(dev.rp.scr, bridge.scr, RP_SCR_SIZE) != 0) {
                fail("the consoles have drawn different text", hdr, addr);
            }
            ++console_cnt;
        }

        uint32_t steps = rnd() % 4;
        for (uint32_t i = 0; i < steps; ++i) {
            step();
        }
    }

    if (memcmp(dev.mem.ram, bridge_ram, RAM_SIZE) != 0) {
        fail("RAM differs at the end", 0, 0);
    }
    if (memcmp(dev.mem.high, bridge_high, HIGH_SIZE) != 0) {
        fail("the high RAM differs at the end", 0, 0);
    }
    if (memcmp(dev_disk, bridge_disk, sizeof(dev_disk)) != 0) {
        fail("the disks differ at the end", 0, 0);
    }
    if (memcmp(dev.rp.scr, bridge.scr, RP_SCR_SIZE) != 0) {
        fail("the frame memories differ at the end", 0, 0);
    }

    // A run that has not met every answer has proven less than it says
    if (stat_cnt[BUS_STAT_OK] == 0 || stat_cnt[BUS_STAT_ERROR] == 0 || stat_cnt[BUS_STAT_PROTECTION] == 0 ||
        stat_cnt[BUS_STAT_BAD_NEXT] == 0 || lost_cnt < 100 || no_data_cnt < 100 || full_cnt < 100 ||
        disk_cnt - disk_error_cnt < 100 || disk_error_cnt < 100 || frame_cnt < 100 || press_cnt < 100 ||
        user_cnt < 100 || console_cnt < 100) {
        fprintf(stderr, "lost answers %llu, disk commands %llu, with an error %llu, reads of an empty console %llu, bytes for a full one %llu\n",
                lost_cnt, disk_cnt, disk_error_cnt, no_data_cnt, full_cnt);
        fail("the requests have not met every answer", 0, 0);
    }

    printf("passed: devices_test (%d requests: %llu done, %llu errors, %llu refused for protection, "
           "%llu without a next word, %llu lost answers, %llu disk commands, %llu of them with an error, "
           "%llu frames on the screen, %llu presses of the button, %llu accesses of a user program to the screen, "
           "%llu bytes of the console on the screen)\n",
           REQUESTS, stat_cnt[BUS_STAT_OK], stat_cnt[BUS_STAT_ERROR], stat_cnt[BUS_STAT_PROTECTION],
           stat_cnt[BUS_STAT_BAD_NEXT], lost_cnt, disk_cnt, disk_error_cnt, frame_cnt, press_cnt, user_cnt, console_cnt);

    return EXIT_SUCCESS;
}
