/*
    A program that checks the kernel from the side of a process that works.

    The kernel gives the stack its pages when the program touches them: each
    first touch is a page fault that the kernel repairs, and the program does
    not notice.

    The kernel takes the CPU from the program at every timer interrupt and
    gives it to another process. The program must not notice that either: its
    registers and its memory are the same when it runs again. The program
    makes a child, so that two processes want the CPU. Both add up a series
    whose sum is known, and they stop when enough timer interrupts have
    arrived. `work_asm.s` checks the registers that C code does not use.
*/

#include "user.h"

#include "check.h"

#define PAGE_SIZE 0x1000

#define STACK_PAGES 3 // pages of the stack that `use_stack` touches

/*
    The stack of a program ends at 8 MB and may grow to 64 KB. This border of
    two pages is in the middle of that range, far below the part of the stack
    that the program uses.
*/
#define FAR_BORDER 0x007f8000

#define ROUNDS_PER_CHECK 100
#define WORK_TICKS 16    // timer interrupts during which a process adds
#define REGISTER_TICKS 8 // timer interrupts during which the registers are checked

/* Functions of `work_asm.s` */

// Returns 0 if the kernel has kept every register through `ticks` timer interrupts.
int keep_registers(int ticks);

// Makes a system call with a number that the kernel does not have, and returns its result.
int unknown_call(void);

int main(int argc, char *argv[]);

/*
    Variables of the program. The linker puts both into the part of the file
    that may be written. The first one has its value in the file. The others
    have no bytes in the file: the kernel gives them memory that is full of
    zeros.
*/
static unsigned int checks_done = 0x1234;
static unsigned int zeros[16];

static int is_zero(const volatile unsigned int *words, int count)
{
    for (int i = 0; i < count; ++i) {
        if (words[i] != 0) {
            return 0;
        }
    }

    return 1;
}

/*
    Reads the variables and writes them. Returns 0 if they are right.
*/
static int use_variables(void)
{
    if (checks_done != 0x1234 || !is_zero(zeros, sizeof(zeros) / sizeof(zeros[0]))) {
        return 1;
    }

    checks_done = 1;
    zeros[15] = 0x5678;

    return checks_done == 1 && zeros[15] == 0x5678 ? 0 : 1;
}

/*
    Writes a byte into each page of an array that is new on the stack, and
    reads the bytes back. Returns 0 if they are right.
*/
static int use_stack(void)
{
    volatile char bytes[STACK_PAGES * PAGE_SIZE];

    for (int i = 0; i < STACK_PAGES; ++i) {
        bytes[i * PAGE_SIZE] = (char) ('a' + i);
    }
    for (int i = 0; i < STACK_PAGES; ++i) {
        if (bytes[i * PAGE_SIZE] != (char) ('a' + i)) {
            return 1;
        }
    }

    return 0;
}

/*
    Adds the odd numbers 3, 5, 7, and so on, until the timer has interrupted
    the machine `ticks` times. The sum of the first `n` of them is
    `n * n + 2 * n`. Returns 0 if the sum is right.
*/
static int work(int ticks)
{
    int start = times(NULL);

    unsigned int count = 0;
    unsigned int sum = 0;
    do {
        for (int i = 0; i < ROUNDS_PER_CHECK; ++i) {
            ++count;
            sum += 2 * count + 1;
        }
    } while (times(NULL) - start < ticks);

    return sum == count * count + 2 * count ? 0 : 1;
}

/*
    Checks the answers of the kernel to requests that it must refuse: a
    program may ask for anything, and the kernel must stay whole. Returns 0
    if every answer is right.
*/
static int check_refusals(void)
{
    char byte = 'x';

    // Memory that is not a part of the program: the address 0, the kernel, and the page after the stack
    if (write(STDOUT, (const void *) 0, 1) != -EFAULT ||
        write(STDOUT, (const void *) 0x80001000, 1) != -EFAULT ||
        write(STDOUT, (const void *) 0x00800000, 1) != -EFAULT) {
        return 1;
    }

    // The code of a program cannot be written
    if (times((tms_t *) main) != -EFAULT) {
        return 1;
    }

    if (write(7, &byte, 1) != -EBADF) {
        return 1;
    }
    if (unknown_call() != -ENOSYS) {
        return 1;
    }

    // A write of no bytes is not a mistake
    if (write(STDOUT, &byte, 0) != 0) {
        return 1;
    }

    return 0;
}

/*
    Gives the kernel a buffer that lies on two pages, in a part of the stack
    that the program has not touched.

    The buffer for `stat` has its first 12 bytes before the border of the
    pages and the rest behind it. The kernel writes there before the program
    does, so it is the kernel that gives the stack the two pages. The rest of
    the two pages must be zeros: a page comes to a program without the bytes
    of the program that had it before. `name` is the name of a file. Returns
    0 if everything is right.
*/
static int use_far_pages(const char *name)
{
    stat_t *info = (stat_t *) (FAR_BORDER - 12);

    if (stat(name, info) != 0 || info->nlink != 1 || (info->mode & S_IFMT) != S_IFREG || info->size == 0) {
        return 1;
    }

    // The first page up to the buffer, and the second page after the buffer
    if (!is_zero((unsigned int *) (FAR_BORDER - PAGE_SIZE), (PAGE_SIZE - 12) / 4) ||
        !is_zero((unsigned int *) (FAR_BORDER - 12 + sizeof(stat_t)), (PAGE_SIZE + 12 - sizeof(stat_t)) / 4)) {
        return 1;
    }

    return 0;
}

/*
    Prints a line from a buffer on the same border of two pages. Returns 0 if
    the kernel has printed all of it.
*/
static int print_from_far_pages(const char *text)
{
    char *line = (char *) (FAR_BORDER - 8);

    int len = 0;
    for (; text[len] != '\0'; ++len) {
        line[len] = text[len];
    }

    return write(STDOUT, line, len) == len ? 0 : 1;
}

int main(int argc, char *argv[])
{
    check(use_variables() == 0, "the variables have their values");
    check(use_stack() == 0, "the new pages of the stack keep their bytes");
    check(check_refusals() == 0, "the kernel refuses wrong requests");
    check(use_far_pages(argv[0]) == 0, "the kernel writes a buffer that lies on the border of two pages");
    check(print_from_far_pages("a line from a buffer on two pages\n") == 0, "the kernel reads such a buffer");

    /*
        The child does the same work at the same time. A process is given
        the CPU for the time between two timer interrupts, so each of the two
        has run for less time than has passed.
    */
    tms_t before;
    int start = times(&before);

    int pid = fork();
    if (pid == 0) {
        exit(keep_registers(REGISTER_TICKS) != 0 || work(WORK_TICKS) != 0 ? 1 : 0);
    }

    int registers_result = keep_registers(REGISTER_TICKS);
    int work_result = work(WORK_TICKS);

    tms_t after;
    int passed = times(&after) - start;

    int status = -1;
    int waited_pid = waitpid(pid, &status, 0);

    check(registers_result == 0, "the registers keep their values");
    check(work_result == 0, "the timer has interrupted the work, and the sum is right");
    check(pid > 0 && waited_pid == pid && status == 0, "a child has done the same work at the same time");
    check(after.utime > before.utime && (int) (after.utime - before.utime) < passed, "the two processes took turns");

    return get_result();
}
