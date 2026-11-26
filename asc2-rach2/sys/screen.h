/*
    The screen and the push button of the RP2040 for user programs.

    The RP2040 shows the text of the console on the screen by itself. One
    program at a time may take the screen from the console: the kernel then
    puts the page of the screen's devices into the memory of the program,
    and the program draws and reads the button with no further help of the
    kernel. When the program gives the screen back or ends in any way, the
    console has the screen again.
*/

#ifndef _SCREEN_H_
#define _SCREEN_H_

#include "sys/lib/kint.h"
#include "sys/proc/process.h"

// Requests of the system call `screen`
#define SCREEN_OPEN    0 // take the screen; the result is the address of the device page in the program
#define SCREEN_CLOSE   1 // give the screen back to the console
#define SCREEN_PICTURE 2 // name a picture of the program: `a` is its number, from 0 to 3, and `b` its address

/*
    int screen(int request, unsigned int a, unsigned int b)

    Does a request of the list above for the running process. Returns the
    address of the device page for `SCREEN_OPEN`, 0 for the other requests,
    or a negative number of an error: `-EBUSY` if another process has the
    screen, `-EINVAL` for a request of a process that does not have it, for
    an unknown request, and for a picture whose number, address, or size
    is wrong, and `-EFAULT` for a picture outside the memory of the program.
*/
int32_t screen_call(uint32_t request, uint32_t a, uint32_t b);

// Gives the screen back to the console if the process has it.
void release_screen(process_t *proc);

#endif
