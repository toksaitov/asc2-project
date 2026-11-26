/*
    Virtual memory.

    A process has a top page table of 1024 entries. An entry in use names a
    second-level table, whose 1024 entries name the pages of the program. An
    address selects an entry of the top table with its bits 31-22, an entry of
    the second-level table with its bits 21-12, and a byte of the page with
    the rest. An entry holds the physical address of a page in its upper bits
    and the permissions in the bits P (in use), W (may be written), and U (a
    user program may use it).

    The CPU reads the tables by itself and remembers the last translations. It
    never remembers a missing one, so the kernel may fill an empty entry at
    any time. A change of an entry in use is seen after a write to `cr3`.

    The kernel runs at addresses that need no tables, so it has no table of
    its own.

    The stack of a program gets its pages when the program first touches
    them. The touch of a missing page raises a page fault; the handler adds
    the page, and the CPU runs the instruction again.
*/

#include "sys/mm/paging.h"

#include <stddef.h>

#include "sys/cpu.h"
#include "sys/isr.h"
#include "sys/lib/kstring.h"
#include "sys/mm/pages.h"
#include "sys/proc/process.h"

#define PT_ENTRIES 1024

// The entries of a top table that a program can have: its addresses end with its stack
#define PT_USER_TOP_ENTRIES (USER_STACK_TOP >> 22)

#define PT_TOP_INDEX(addr)    ((addr) >> 22)
#define PT_SECOND_INDEX(addr) (((addr) >> 12) & 0x3ff)

#define PTE_ADDR_MASK 0xfffff000 // physical address of the page in an entry
#define PAGE_OFF_MASK (PAGE_SIZE - 1)

static uint32_t repaired_faults;

// A page of device registers in the memory of a program: it is no page of RAM
static bool is_device_entry(uint32_t entry)
{
    return (entry & PTE_ADDR_MASK) == RP_USER_BASE;
}

uint32_t *create_page_dir(void)
{
    return (uint32_t *) alloc_page();
}

/*
    Returns the index of the first entry of a second-level table, at `index`
    or after it, that has a page, or `PT_ENTRIES` if there is none.

    A program has a few pages at the start of its memory and a few at the
    end, so nearly all of the 1024 entries between them are empty. The search
    passes four empty entries in one step where a group of four starts.
*/
static uint32_t find_page(const uint32_t *table, uint32_t index)
{
    while (index < PT_ENTRIES && !(table[index] & PTE_P)) {
        if ((index & 3) == 0 && !((table[index + 1] | table[index + 2] | table[index + 3]) & PTE_P)) {
            index += 4;
        } else {
            ++index;
        }
    }

    return index;
}

void free_page_dir(uint32_t *page_dir)
{
    for (uint32_t i = 0; i < PT_USER_TOP_ENTRIES; ++i) {
        if (!(page_dir[i] & PTE_P)) {
            continue;
        }

        uint32_t *table = (uint32_t *) PHYS_TO_KERN(page_dir[i] & PTE_ADDR_MASK);
        for (uint32_t j = find_page(table, 0); j < PT_ENTRIES; j = find_page(table, j + 1)) {
            if (!is_device_entry(table[j])) {
                free_page(PHYS_TO_KERN(table[j] & PTE_ADDR_MASK));
            }
        }
        free_page(table);
    }

    free_page(page_dir);
}

uint32_t *copy_page_dir(uint32_t *page_dir)
{
    uint32_t *copy = create_page_dir();
    if (copy == NULL) {
        return NULL;
    }

    for (uint32_t i = 0; i < PT_USER_TOP_ENTRIES; ++i) {
        if (!(page_dir[i] & PTE_P)) {
            continue;
        }

        uint32_t *table = (uint32_t *) PHYS_TO_KERN(page_dir[i] & PTE_ADDR_MASK);
        uint32_t *table_copy = (uint32_t *) alloc_page();
        if (table_copy == NULL) {
            free_page_dir(copy);
            return NULL;
        }
        copy[i] = KERN_TO_PHYS(table_copy) | (page_dir[i] & ~PTE_ADDR_MASK);

        for (uint32_t j = find_page(table, 0); j < PT_ENTRIES; j = find_page(table, j + 1)) {
            // The screen stays with the parent
            if (is_device_entry(table[j])) {
                continue;
            }

            // A page that the program may not write, a page of its code, is one page for both processes
            if (!(table[j] & PTE_W)) {
                share_page(PHYS_TO_KERN(table[j] & PTE_ADDR_MASK));
                table_copy[j] = table[j];
                continue;
            }

            // The whole page is written at once, so it need not be cleared first
            void *page = take_page();
            if (page == NULL) {
                free_page_dir(copy);
                return NULL;
            }
            memcpy(page, PHYS_TO_KERN(table[j] & PTE_ADDR_MASK), PAGE_SIZE);
            table_copy[j] = KERN_TO_PHYS(page) | (table[j] & ~PTE_ADDR_MASK);
        }
    }

    return copy;
}

/*
    Returns the entry of the second-level table for the page of `addr`, or
    NULL if there is no second-level table. With `create` set, a missing table
    is made first, and NULL means that no page is free.
*/
static uint32_t *get_pte(uint32_t *page_dir, uint32_t addr, bool create)
{
    uint32_t *top_entry = &page_dir[PT_TOP_INDEX(addr)];

    if (!(*top_entry & PTE_P)) {
        if (!create) {
            return NULL;
        }

        void *table = alloc_page();
        if (table == NULL) {
            return NULL;
        }

        // An access needs the permission in both entries, so the top entry allows everything
        *top_entry = KERN_TO_PHYS(table) | PTE_P | PTE_W | PTE_U;
    }

    uint32_t *table = (uint32_t *) PHYS_TO_KERN(*top_entry & PTE_ADDR_MASK);

    return &table[PT_SECOND_INDEX(addr)];
}

void *map_user_page(uint32_t *page_dir, uint32_t addr, bool writable)
{
    uint32_t *pte = get_pte(page_dir, addr, true);
    if (pte == NULL) {
        return NULL;
    }

    if (!(*pte & PTE_P)) {
        void *page = alloc_page();
        if (page == NULL) {
            return NULL;
        }

        *pte = KERN_TO_PHYS(page) | PTE_P | PTE_U;
    }
    if (writable) {
        *pte |= PTE_W;
    }

    return PHYS_TO_KERN(*pte & PTE_ADDR_MASK);
}

bool map_device_page(uint32_t *page_dir, uint32_t addr, uint32_t phys)
{
    uint32_t *pte = get_pte(page_dir, addr, true);
    if (pte == NULL || (*pte & PTE_P)) {
        return false;
    }
    *pte = phys | PTE_P | PTE_W | PTE_U;

    return true;
}

void unmap_device_page(uint32_t *page_dir, uint32_t addr)
{
    uint32_t *pte = get_pte(page_dir, addr, false);
    if (pte != NULL && is_device_entry(*pte)) {
        *pte = 0;
    }
}

static bool is_stack_addr(uint32_t addr)
{
    return addr >= USER_STACK_START && addr < USER_STACK_TOP;
}

/*
    Checks that the program may read the byte at `addr`, or write it if
    `write` is set, and returns the address of the byte in the kernel's
    window. Returns NULL if the program may not.
*/
static void *get_user_byte(uint32_t *page_dir, uint32_t addr, bool write)
{
    uint32_t *pte = get_pte(page_dir, addr, false);
    if (pte == NULL || !(*pte & PTE_P)) {
        // A part of the stack that the program has not touched yet
        if (!is_stack_addr(addr) || map_user_page(page_dir, addr, true) == NULL) {
            return NULL;
        }
        pte = get_pte(page_dir, addr, false);
    }

    // An access needs the permission in both entries, as the CPU checks it
    uint32_t allowed = *pte & page_dir[PT_TOP_INDEX(addr)];
    if (!(allowed & PTE_U) || (write && !(allowed & PTE_W)) || is_device_entry(*pte)) {
        return NULL;
    }

    return (uint8_t *) PHYS_TO_KERN(*pte & PTE_ADDR_MASK) + (addr & PAGE_OFF_MASK);
}

void *get_user_bytes(uint32_t *page_dir, uint32_t addr, bool write, uint32_t *size)
{
    // The bytes up to the end of the page are next to each other in RAM; the next page may be anywhere
    uint32_t count = PAGE_SIZE - (addr & PAGE_OFF_MASK);
    if (*size > count) {
        *size = count;
    }

    return get_user_byte(page_dir, addr, write);
}

/*
    Copies `size` bytes between the memory of a program at `user_addr` and the
    kernel's bytes at `kernel_bytes`, into the program if `to_user` is set.
*/
static bool copy_user(uint32_t *page_dir, uint32_t user_addr, uint8_t *kernel_bytes, uint32_t size, bool to_user)
{
    while (size > 0) {
        uint32_t count = size;
        uint8_t *user_bytes = (uint8_t *) get_user_bytes(page_dir, user_addr, to_user, &count);
        if (user_bytes == NULL) {
            return false;
        }

        if (to_user) {
            memcpy(user_bytes, kernel_bytes, count);
        } else {
            memcpy(kernel_bytes, user_bytes, count);
        }

        user_addr += count;
        kernel_bytes += count;
        size -= count;
    }

    return true;
}

bool copy_from_user(uint32_t *page_dir, void *dst, uint32_t src, uint32_t size)
{
    return copy_user(page_dir, src, (uint8_t *) dst, size, false);
}

bool copy_to_user(uint32_t *page_dir, uint32_t dst, const void *src, uint32_t size)
{
    return copy_user(page_dir, dst, (uint8_t *) src, size, true);
}

int32_t copy_str_from_user(uint32_t *page_dir, char *dst, uint32_t src, uint32_t size)
{
    uint32_t len = 0;

    while (len < size) {
        uint32_t count = size - len;
        const char *user_bytes = (const char *) get_user_bytes(page_dir, src + len, false, &count);
        if (user_bytes == NULL) {
            return COPY_STR_FAULT;
        }

        for (uint32_t i = 0; i < count; ++i) {
            dst[len] = user_bytes[i];
            if (dst[len] == '\0') {
                return (int32_t) len;
            }
            ++len;
        }
    }

    return COPY_STR_TOO_LONG;
}

/*
    A page fault in a user program is repaired if the program has touched a
    missing page of its stack. Any other page fault is a mistake of the
    program, or of the kernel if the kernel was interrupted.

    A page is missing if its entry is absent. The cause of a page fault also
    has the present bit clear when an entry was malformed or when the RP2040
    refused the read of an entry. A new page repairs neither, and the access
    would fault again without an end.
*/
static void page_fault_handler(isr_params_t *params)
{
    bool is_user = (params->flags & CPU_FLAG_U) != 0;
    bool is_missing = !(params->cause & (CPU_CAUSE_P | CPU_CAUSE_M | CPU_CAUSE_STAT_MASK));

    if (is_user && is_missing && is_stack_addr(params->fault_addr) &&
        map_user_page(current_proc->page_dir, params->fault_addr, true) != NULL) {
        ++repaired_faults;
        return;
    }

    fail_isr(params);
}

void init_paging(void)
{
    register_isr_handler(CPU_EXCPT_PAGE_FAULT, page_fault_handler);

    // With paging, the chip's cache of instructions: a trap and the return
    // from it empty the cache, so a program that the kernel has loaded or
    // changed never runs from stale words
    write_cr0(read_cr0() | CPU_CTRL_P | CPU_CTRL_C);
}

uint32_t get_repaired_faults(void)
{
    return repaired_faults;
}
