/*
    The disk of the RP2040, as a row of bytes.
*/

#ifndef _DISK_H_
#define _DISK_H_

#include <stdbool.h>

#include "sys/lib/kint.h"

// Finds the size of the disk. Lets the disk interrupt the CPU.
void init_disk(void);

// Returns the size of the disk in bytes, 0 if there is no disk.
uint32_t get_disk_size(void);

// Copies `size` bytes of the disk from the position `pos` to `dst`. Returns false if the disk has failed.
bool read_disk(uint32_t pos, void *dst, uint32_t size);

/*
    Copies `size` bytes from `src` to the position `pos` of the disk. The
    bytes reach the disk itself at the next `sync_disk`, or earlier. Returns
    false if the disk has failed.
*/
bool write_disk(uint32_t pos, const void *src, uint32_t size);

// Writes every changed byte to the disk itself. Returns false if the disk has failed.
bool sync_disk(void);

// Wakes the process that waits for the end of a command. The handler of the RP2040's interrupt line calls it.
void handle_disk_irq(void);

#endif
