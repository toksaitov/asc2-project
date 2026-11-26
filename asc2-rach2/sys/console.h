/*
    The console of the RP2040: a terminal on the other side of the
    microcontroller.
*/

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "sys/lib/kint.h"

// Prints a byte. Waits until the console can take it.
void console_putc(char ch);

/*
    Gives a program the bytes of a typed line: at most `count` of them go to
    the address `buf` of the program with the page tables `page_dir`. The
    process sleeps until the line is complete. A longer line stays for the
    next reads. Returns the number of bytes, 0 at the end of the input, or
    a negative number of an error.
*/
int32_t read_console(uint32_t *page_dir, uint32_t buf, uint32_t count);

// Takes the bytes that have been typed. The handler of the RP2040's interrupt line calls it.
void handle_console_irq(void);

#endif
