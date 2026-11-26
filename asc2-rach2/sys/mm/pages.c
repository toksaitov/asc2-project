/*
    The pages of RAM that the kernel gives out.

    RAM starts with the page of the interrupt vector table and the boot stack.
    The variables of the kernel follow up to the symbol `_end` of the linker.
    Every page after that is free at the start. The amount of RAM depends on
    the board, and a register of the RP2040 holds it.

    The free pages form a list: the first word of a free page is the address
    of the next free page, and the last one has NULL there. The list needs no
    memory of its own.

    A page in use may have more than one user: processes that run the same
    program share the pages that none of them may write. A byte for every
    page counts the users after the first one.
*/

#include "sys/mm/pages.h"

#include <stddef.h>

#include "sys/lib/kstring.h"

extern char _end[]; // the first byte after the variables of the kernel

#define MAX_PAGES 256 // the kernel gives out this many pages of RAM at the most: 1 MB

static void *free_list;
static uint32_t free_pages;
static uint32_t least_free_pages;

// How many users a page has after its first one, by the number of the page in RAM
static uint8_t more_users[MAX_PAGES];

static uint32_t get_page_number(void *page)
{
    return KERN_TO_PHYS(page) / PAGE_SIZE;
}

uint32_t get_ram_size(void)
{
    volatile uint32_t *ram_size = (volatile uint32_t *) (KERN_BASE + RP_RAM_SIZE_ADDR);

    return *ram_size;
}

void init_pages(void)
{
    uint32_t first = ((uint32_t) _end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t ram_size = get_ram_size();
    if (ram_size > MAX_PAGES * PAGE_SIZE) {
        ram_size = MAX_PAGES * PAGE_SIZE;
    }
    uint32_t end = KERN_BASE + ram_size;

    // The pages are added from the last one, so that the first one is taken first
    for (uint32_t page = end - PAGE_SIZE; page >= first; page -= PAGE_SIZE) {
        free_page((void *) page);
    }

    least_free_pages = free_pages;
}

void *take_page(void)
{
    void *page = free_list;
    if (page == NULL) {
        return NULL;
    }

    free_list = *(void **) page;
    --free_pages;
    if (free_pages < least_free_pages) {
        least_free_pages = free_pages;
    }

    return page;
}

void *alloc_page(void)
{
    void *page = take_page();
    if (page != NULL) {
        memset(page, 0, PAGE_SIZE);
    }

    return page;
}

void share_page(void *page)
{
    ++more_users[get_page_number(page)];
}

void free_page(void *page)
{
    uint8_t *others = &more_users[get_page_number(page)];
    if (*others > 0) {
        --*others;
        return;
    }

    *(void **) page = free_list;
    free_list = page;
    ++free_pages;
}

uint32_t get_free_pages(void)
{
    return free_pages;
}

uint32_t get_least_free_pages(void)
{
    return least_free_pages;
}
