/*
    A program that gives the scheduler processes of three kinds to choose
    between, as a load to measure it with (`sys/proc/slice.c` of the kernel).

    * The program itself and one child compute without a pause.
    * Two children write a file again and again. The kernel puts a process
      to sleep while the disk writes, so they give the CPU back by
      themselves most of the time.
    * One child computes for a while between two writes.

    The machine has memory for five such processes and the shell, and no
    more. Every process has a fixed amount of work, so the same work is done whatever
    the scheduler decides, and only the time and the order differ.
*/

#include "user.h"

#include "check.h"

#define CHILDREN 4

#define LONG_SUM   150000 // numbers that a computing child adds
#define SHORT_SUM  2500   // numbers that the mixed child adds between two writes
#define WRITES     24     // writes of a writing child
#define MIXED_WRITES 12   // writes of the mixed child

// Adds the numbers from 1 to `count`. Returns 0 if the sum is right. The sum may pass the largest number and wrap around.
static int add_numbers(unsigned int count)
{
    volatile unsigned int sum = 0;
    for (unsigned int i = 1; i <= count; ++i) {
        sum += i;
    }

    return 2 * sum == count * (count + 1) ? 0 : 1;
}

/*
    Writes a line to the start of the file `name`, `writes` times, and adds
    `sum` numbers before each write. Returns 0 if everything went right.
*/
static int write_file(const char *name, int writes, unsigned int sum)
{
    static const char line[] = "a line that the load writes to the disk\n";

    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return 1;
    }

    int failures = 0;
    for (int i = 0; i < writes; ++i) {
        failures += add_numbers(sum);
        failures += lseek(fd, 0, SEEK_SET) != 0;
        failures += write(fd, line, sizeof(line) - 1) != sizeof(line) - 1;
    }
    failures += close(fd) != 0;
    failures += unlink(name) != 0;

    return failures == 0 ? 0 : 1;
}

// Does the work of the child with the number `i`. Returns 0 if everything went right.
static int work(int i)
{
    switch (i) {
        case 0:
            return add_numbers(LONG_SUM);
        case 1:
            return write_file("load1.txt", WRITES, 0);
        case 2:
            return write_file("load2.txt", WRITES, 0);
        default:
            return write_file("load3.txt", MIXED_WRITES, SHORT_SUM);
    }
}

int main(void)
{
    int pids[CHILDREN];
    for (int i = 0; i < CHILDREN; ++i) {
        pids[i] = fork();
        if (pids[i] == 0) {
            exit(work(i));
        }
    }

    int failures = add_numbers(LONG_SUM);
    for (int i = 0; i < CHILDREN; ++i) {
        int status = -1;
        failures += pids[i] <= 0 || waitpid(pids[i], &status, 0) != pids[i] || status != 0;
    }

    check(failures == 0, "five processes of three kinds have done their work");

    // The numbers themselves depend on when the interrupts arrive, so the check is of what must hold for all of them
    unsigned int words[SLICES_WORDS] = { 0, 0, 0, 0 };
    check(slices(SLICES_READ, words) == 0 && words[SLICES_TURNS] > 0 && words[SLICES_RIGHT] <= words[SLICES_TURNS] &&
          words[SLICES_SLEEPS] > 0 && words[SLICES_SLEEPS] < words[SLICES_TURNS] && words[SLICES_WEIGHTS] != 0,
          "the kernel tells what the network of its scheduler has learned");
    int before = times(NULL);
    check(nap(3) == 0 && times(NULL) - before >= 3 && nap(0) == 0, "a process sleeps for three periods of the timer");

    check(slices(2, words) == -EINVAL && slices(SLICES_READ, (unsigned int *) 0x80001000) == -EFAULT,
          "the kernel refuses a request that it does not know and an address outside the program");

    return get_result();
}
