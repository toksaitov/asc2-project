/*
    The end of the kernel when it finds a mistake of its own.
*/

#ifndef _PANIC_H_
#define _PANIC_H_

// Prints the message and stops the machine. Does not return.
void k_panic(const char *message);

#endif
