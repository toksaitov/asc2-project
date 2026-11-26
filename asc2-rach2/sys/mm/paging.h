/*
    Virtual memory: the page tables of a process and the memory of its
    program.
*/

#ifndef _PAGING_H_
#define _PAGING_H_

#include <stdbool.h>

#include "sys/machine.h"
#include "sys/lib/kint.h"

/*
    The addresses of a program.

    The lower half of the address space belongs to the running program, and
    the CPU translates it through the page tables of the process. The linker
    places a program at 4 MB. The stack ends at 8 MB and grows down, so a
    program needs one second-level table for its code, its data, and its
    stack. The page at the address 0 is never mapped: a program that uses a
    NULL pointer is stopped.
*/
#define USER_START       PAGE_SIZE
#define USER_STACK_TOP   0x00800000
#define USER_STACK_SIZE  0x00010000 // a stack may grow to this size
#define USER_STACK_START (USER_STACK_TOP - USER_STACK_SIZE)

// The page below the stack is the place of the device page of a program that has the screen (`sys/screen.h`)
#define USER_DEVICE_ADDR (USER_STACK_START - PAGE_SIZE)

// Sets the handler of page faults and turns paging on.
void init_paging(void);

// Returns the number of page faults that the kernel has repaired.
uint32_t get_repaired_faults(void);

// Makes an empty top page table. Returns NULL if no page is free.
uint32_t *create_page_dir(void);

// Frees every page of the program, the second-level tables, and the top table.
void free_page_dir(uint32_t *page_dir);

/*
    Makes page tables with a copy of every page of a program, for `fork`.
    Returns the new top table, or NULL if the free pages have run out.
*/
uint32_t *copy_page_dir(uint32_t *page_dir);

/*
    Gives the page of `addr` a new page of zeros, unless it has one. The
    program may read it, and write it if `writable` is set; a page that is
    there already becomes writable if `writable` is set. Returns the page's
    address in the kernel's window, or NULL if no page is free.
*/
void *map_user_page(uint32_t *page_dir, uint32_t addr, bool writable);

/*
    Puts the device page at the physical address `phys` into the memory of a
    program at `addr`, for reading and writing. The page is no RAM: it is
    neither copied by `copy_page_dir` nor freed by `free_page_dir`, and the
    kernel takes no bytes of a program from it. Returns false if the address
    is in use or no page is free for a table.
*/
bool map_device_page(uint32_t *page_dir, uint32_t addr, uint32_t phys);

// Takes a device page out of the memory of a program. The CPU forgets it at the next write to `cr3`.
void unmap_device_page(uint32_t *page_dir, uint32_t addr);

/*
    The kernel reaches the memory of a program only through the functions
    below. They look the addresses up in the page tables as the CPU does, so a
    wrong address from a program is refused and cannot make the kernel fault.
*/

// Copies bytes of a program into the kernel. Returns false if the program may not read all of them.
bool copy_from_user(uint32_t *page_dir, void *dst, uint32_t src, uint32_t size);

// Copies bytes of the kernel into the memory of a program. Returns false if the program may not write all of them.
bool copy_to_user(uint32_t *page_dir, uint32_t dst, const void *src, uint32_t size);

/*
    Copies a string of a program into the `size` bytes at `dst`, with its
    zero byte. Returns the length of the string, `COPY_STR_FAULT` if the
    program may not read all of it, or `COPY_STR_TOO_LONG` if it does not fit.
*/
#define COPY_STR_FAULT    (-1)
#define COPY_STR_TOO_LONG (-2)
int32_t copy_str_from_user(uint32_t *page_dir, char *dst, uint32_t src, uint32_t size);

/*
    Returns the address in the kernel's window of the program's byte at
    `addr`, for a caller that reads the bytes of the program in place, or
    writes them if `write` is set. `size` is the number of bytes that the
    caller wants; it becomes the number of bytes that are next to each other
    in RAM from that address on, which end with the page. Returns NULL if the
    program may not use the byte.
*/
void *get_user_bytes(uint32_t *page_dir, uint32_t addr, bool write, uint32_t *size);

#endif
