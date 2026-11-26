/*
    A program with mistakes.

    The program makes five children, one after another. Each of them does
    something that the CPU refuses with an exception. The kernel stops the
    child and says why, and the parent learns it from `waitpid`.

    * A write through a NULL pointer. The page at the address 0 is never a
      part of a program, so the write raises a page fault that the kernel
      cannot repair.
    * A write into the code of the program, whose pages are for reading.
    * A call of a function at an address of the kernel. A program has no
      access to that part of memory.
    * `cli`, an instruction for the kernel only.
    * `int 0x81`, a number that the kernel does not use.
*/

#include "user.h"

#include "check.h"

#define MISTAKES 5

#define KERNEL_CODE 0x80800000 // address of the first instruction of the kernel

/* Functions of `crash_asm.s` */

// Runs `cli`.
void turn_interrupts_off(void);

// Runs `int 0x81`.
void enter_kernel_wrongly(void);

int main(void);

static void make_mistake(int number)
{
    volatile int *nowhere = (volatile int *) 0;
    volatile int *code = (volatile int *) main;
    void (*kernel_code)(void) = (void (*)(void)) KERNEL_CODE;

    if (number == 0) {
        print(STDOUT, "a child writes to the address 0\n");
        *nowhere = 1;
    } else if (number == 1) {
        print(STDOUT, "a child writes into its code\n");
        *code = 1;
    } else if (number == 2) {
        print(STDOUT, "a child calls the kernel as a function\n");
        kernel_code();
    } else if (number == 3) {
        print(STDOUT, "a child turns interrupts off\n");
        turn_interrupts_off();
    } else {
        print(STDOUT, "a child enters the kernel with a number that the kernel does not use\n");
        enter_kernel_wrongly();
    }
}

int main(void)
{
    for (int i = 0; i < MISTAKES; ++i) {
        int pid = fork();
        if (pid == 0) {
            make_mistake(i);
            exit(0);
        }

        // A child that has gone on to its end has the status 0
        int status = 0;
        int waited_pid = waitpid(pid, &status, 0);

        check(pid > 0 && waited_pid == pid && STOP_REASON(status) != 0, "the kernel has stopped the child");
    }

    return get_result();
}
