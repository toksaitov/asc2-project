/*
    The pages of RAM that the kernel gives out: to itself for page tables and
    kernel stacks, and to processes for their code, data, and stacks.
*/

#ifndef _PAGES_H_
#define _PAGES_H_

#include "sys/machine.h"
#include "sys/lib/kint.h"

/*
    A page has a physical address, which page tables and the CPU use, and an
    address in the kernel's window, at which the kernel reads and writes it.
    The two differ by `KERN_BASE`.
*/
#define PHYS_TO_KERN(addr) ((void *) ((uint32_t) (addr) + KERN_BASE))
#define KERN_TO_PHYS(addr) ((uint32_t) (addr) - KERN_BASE)

// Makes every page of RAM after the kernel's own variables free.
void init_pages(void);

// Returns the amount of RAM in bytes.
uint32_t get_ram_size(void);

// Returns the number of free pages.
uint32_t get_free_pages(void);

// Returns the least number of free pages that there has been since the start.
uint32_t get_least_free_pages(void);

// Takes a free page and fills it with zeros. Returns its address in the kernel's window, or NULL if no page is free.
void *alloc_page(void);

/*
    Takes a free page with the bytes that its last user has left, for a
    caller that fills the whole page by itself. Returns NULL if no page is
    free.
*/
void *take_page(void);

/*
    Gives a page one user more. A page that no program may write, such as a
    page of code, is the same for a process and its copy, so `fork` shares it
    instead of copying it.
*/
void share_page(void *page);

// Gives a page back: it is free again when its last user has given it back.
void free_page(void *page);

#endif
