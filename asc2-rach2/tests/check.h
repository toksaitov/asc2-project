/*
    What the programs that check the kernel share.
*/

#ifndef _CHECK_H_
#define _CHECK_H_

// Prints `text` if `is_right`, and `text` behind `FAILED: ` if not. A failure is remembered.
void check(int is_right, const char *text);

// Returns what a test program ends with: 0 if no check has failed, and 1 otherwise.
int get_result(void);

// Keeps the CPU busy until the timer has interrupted the machine `ticks` times.
void spin(int ticks);

#endif
