/*
    The disk of the RP2040.

    The disk is a row of sectors of 4 KB. The RP2040 copies a whole sector
    between the disk and a buffer in RAM by itself: the kernel writes the
    number of the sector, the physical address of the buffer, and the command
    into three registers, and the status register tells when the command has
    ended. The end also raises the interrupt line of the RP2040.

    A read ends before the CPU can ask twice, so the kernel waits for it in a
    loop. A write takes tens of milliseconds, and the process sleeps until the
    interrupt, so that other processes run meanwhile. Before the first process
    exists there is nobody to sleep, and the kernel waits in a loop.

    The rest of the kernel wants bytes at any position, a few at a time. A
    cache of two sectors stands between: a sector is read into it once, its
    bytes are used there, and a changed sector goes back to the disk when its
    place is needed or when `sync_disk` asks. Two sectors are enough for a
    copy from one place of the disk to another.

    Only one process at a time may use the disk. The file system, the only
    user, sees to that.
*/

#include "sys/disk.h"

#include <stddef.h>

#include "sys/machine.h"
#include "sys/rp.h"
#include "sys/lib/kstring.h"
#include "sys/mm/pages.h"
#include "sys/proc/process.h"
#include "sys/proc/sched.h"

#define SEC_SIZE RP_DISK_SEC_SIZE

/*
    A position on the disk is the number of a sector in its high bits and a
    position in that sector in its low 12 bits. The CPU has no instruction
    that divides, so the two are taken apart with a shift and a mask.
*/
#define SEC_SHIFT 12
#define SEC_OFF_MASK (SEC_SIZE - 1)

#define CACHE_SECTORS 2

#define MAX_SECTORS 0x40000 // sectors of the largest disk that the kernel looks for: 1 GB

typedef struct cache_entry
{
    uint32_t sector;
    bool is_valid;   // the buffer has the bytes of the sector
    bool is_changed; // the buffer has bytes that the disk does not have yet
    uint32_t used;   // when the entry was last used: the larger, the later
} cache_entry_t;

static cache_entry_t cache[CACHE_SECTORS];

// The buffers are made of words: the RP2040 wants the address of a buffer to be a multiple of 4
static uint32_t cache_data[CACHE_SECTORS][SEC_SIZE / sizeof(uint32_t)];

static uint32_t uses;    // counts the uses of the cache
static uint32_t sectors; // size of the disk

/*
    Runs a command on a sector with a buffer and waits for its end. Returns
    false if the disk has refused it.
*/
static bool run_command(uint32_t command, uint32_t sector, void *buf)
{
    write_rp(RP_DISK_SEC_ADDR, sector);
    write_rp(RP_DISK_BUF_ADDR, KERN_TO_PHYS(buf));
    write_rp(RP_DISK_CMD_ADDR, command);

    while (read_rp(RP_DISK_STAT_ADDR) & RP_DISK_FLAG_BUSY) {
        if (command == RP_DISK_CMD_WRITE && current_proc != NULL) {
            sleep(cache);
        }
    }

    // The end of a command asks for an interrupt until the kernel says that it has seen it
    write_rp(RP_IRQ_STAT_ADDR, RP_IRQ_FLAG_DISK);

    return !(read_rp(RP_DISK_STAT_ADDR) & RP_DISK_FLAG_ERROR);
}

void handle_disk_irq(void)
{
    write_rp(RP_IRQ_STAT_ADDR, RP_IRQ_FLAG_DISK);
    wakeup(cache);
}

/*
    The RP2040 has no register with the size of its disk, but it refuses a
    sector that the disk does not have. The numbers 1, 2, 4, and so on are
    tried until one is refused. The size is then between the last two of
    them, and each further try halves that range.
*/
void init_disk(void)
{
    void *buf = cache_data[0];

    sectors = 0;
    if (run_command(RP_DISK_CMD_READ, 0, buf)) {
        uint32_t last = 0;  // a sector that the disk has
        uint32_t first = 1; // a sector that the disk does not have, once the loop has ended
        while (first < MAX_SECTORS && run_command(RP_DISK_CMD_READ, first, buf)) {
            last = first;
            first *= 2;
        }
        while (first - last > 1) {
            uint32_t middle = last + (first - last) / 2;
            if (run_command(RP_DISK_CMD_READ, middle, buf)) {
                last = middle;
            } else {
                first = middle;
            }
        }

        sectors = first;
    }

    for (uint32_t i = 0; i < CACHE_SECTORS; ++i) {
        cache[i].is_valid = false;
        cache[i].is_changed = false;
        cache[i].used = 0;
    }
    uses = 0;

    enable_rp_irq(RP_IRQ_FLAG_DISK);
}

uint32_t get_disk_size(void)
{
    return sectors * SEC_SIZE;
}

static bool write_entry(uint32_t index)
{
    if (cache[index].is_changed) {
        if (!run_command(RP_DISK_CMD_WRITE, cache[index].sector, cache_data[index])) {
            return false;
        }
        cache[index].is_changed = false;
    }

    return true;
}

/*
    Returns the bytes of a sector in the cache, or NULL if the disk has
    failed. The caller sets `is_changed` of the entry if it writes them.
*/
static uint8_t *get_sector(uint32_t sector, cache_entry_t **entry)
{
    uint32_t index = CACHE_SECTORS;
    uint32_t oldest = 0;

    for (uint32_t i = 0; i < CACHE_SECTORS; ++i) {
        if (cache[i].is_valid && cache[i].sector == sector) {
            index = i;
            break;
        }
        if (cache[i].used < cache[oldest].used) {
            oldest = i;
        }
    }

    if (index == CACHE_SECTORS) {
        // The sector takes the place of the one that was not used for the longest time
        index = oldest;
        if (!write_entry(index)) {
            return NULL;
        }

        cache[index].is_valid = false;
        if (sector >= sectors || !run_command(RP_DISK_CMD_READ, sector, cache_data[index])) {
            return NULL;
        }
        cache[index].sector = sector;
        cache[index].is_valid = true;
    }

    cache[index].used = ++uses;
    *entry = &cache[index];

    return (uint8_t *) cache_data[index];
}

/*
    Copies `size` bytes between the disk at `pos` and the kernel's bytes at
    `bytes`, to the disk if `to_disk` is set.
*/
static bool copy_disk(uint32_t pos, uint8_t *bytes, uint32_t size, bool to_disk)
{
    while (size > 0) {
        cache_entry_t *entry;
        uint8_t *data = get_sector(pos >> SEC_SHIFT, &entry);
        if (data == NULL) {
            return false;
        }

        // The bytes up to the end of the sector
        uint32_t offset = pos & SEC_OFF_MASK;
        uint32_t count = SEC_SIZE - offset;
        if (count > size) {
            count = size;
        }

        if (to_disk) {
            memcpy(data + offset, bytes, count);
            entry->is_changed = true;
        } else {
            memcpy(bytes, data + offset, count);
        }

        pos += count;
        bytes += count;
        size -= count;
    }

    return true;
}

bool read_disk(uint32_t pos, void *dst, uint32_t size)
{
    return copy_disk(pos, (uint8_t *) dst, size, false);
}

bool write_disk(uint32_t pos, const void *src, uint32_t size)
{
    return copy_disk(pos, (uint8_t *) src, size, true);
}

bool sync_disk(void)
{
    bool is_done = true;

    for (uint32_t i = 0; i < CACHE_SECTORS; ++i) {
        if (!write_entry(i)) {
            is_done = false;
        }
    }

    return is_done;
}
