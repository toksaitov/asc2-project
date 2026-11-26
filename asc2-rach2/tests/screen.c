/*
    The screen of the machine in the hands of a program.

    The kernel lends the screen to one process at a time: it puts the page
    of the screen's devices into the memory of the process. The program
    checks the rules of the loan:

    * a process that has not asked has no screen, and the kernel refuses its
      requests about pictures;
    * the process that has asked reads and writes the frame memory, and the
      button and the clock are there;
    * the kernel takes no bytes for a file out of the device page;
    * a picture must have a number, an address, and a size that make sense;
    * a child does not get the screen of its parent: the kernel refuses the
      child's own request, and a touch of the page stops the child;
    * the screen is free again after the process has given it back, after
      it has ended, and after the kernel has stopped it.
*/

#include "user.h"

#include "check.h"

#define FRAME      ((volatile unsigned int *) 0x007ef000) // the frame memory in the device page
#define TIME       (*(volatile unsigned int *) 0x007ef420) // the clock of the device page
#define FRAME_SIZE 1024

// A picture of 8 by 2 pixels, and one that says of itself that it is wider than the screen
static unsigned int picture[] = { 8 | 2 << 16, 0x000000ff, 0x00000081 };
static unsigned int wide_picture[] = { 200 | 2 << 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// Runs `work` in a child and returns what `waitpid` says of the child.
static int run_child(int (*work)(void))
{
    int pid = fork();
    if (pid == 0) {
        exit(work());
    }

    int status = -1;
    waitpid(pid, &status, 0);

    return status;
}

static int ask_as_child(void)
{
    return screen(SCREEN_OPEN, 0, 0) == -EBUSY ? 0 : 1;
}

static int touch_as_child(void)
{
    FRAME[0] = 1;

    return 0;
}

static int take_and_end(void)
{
    return screen(SCREEN_OPEN, 0, 0) == (int) FRAME ? 0 : 1;
}

static int take_and_fail(void)
{
    volatile int *nowhere = (volatile int *) 0;

    if (screen(SCREEN_OPEN, 0, 0) != (int) FRAME) {
        return 1;
    }
    *nowhere = 1;

    return 0;
}

int main(void)
{
    check(screen(SCREEN_PICTURE, 0, (unsigned int) picture) == -EINVAL && screen(SCREEN_CLOSE, 0, 0) == -EINVAL,
          "a process without the screen cannot name a picture or give the screen back");

    check(screen(SCREEN_OPEN, 0, 0) == (int) FRAME, "the kernel lends the screen");
    check(screen(SCREEN_OPEN, 0, 0) == (int) FRAME, "a second request of the same process changes nothing");

    FRAME[0] = 0x12345678;
    FRAME[FRAME_SIZE / 4 - 1] = 0x9abcdef0;
    check(FRAME[0] == 0x12345678 && FRAME[FRAME_SIZE / 4 - 1] == 0x9abcdef0, "the frame memory keeps what the program writes");

    unsigned int start = TIME;
    while (TIME == start) {
    }
    check(1, "the clock of the device page runs");

    check(write(STDOUT, (const void *) FRAME, 4) == -EFAULT, "the kernel takes no bytes out of the device page");

    check(screen(SCREEN_PICTURE, 0, (unsigned int) picture) == 0 && screen(SCREEN_PICTURE, 3, (unsigned int) picture) == 0,
          "the kernel takes a picture");
    check(screen(SCREEN_PICTURE, 4, (unsigned int) picture) == -EINVAL, "a picture has a number from 0 to 3");
    check(screen(SCREEN_PICTURE, 0, (unsigned int) picture + 1) == -EINVAL, "a picture starts at a multiple of 4");
    check(screen(SCREEN_PICTURE, 0, (unsigned int) wide_picture) == -EINVAL, "a picture is not larger than the screen");
    check(screen(SCREEN_PICTURE, 0, 0x00100000) == -EFAULT && screen(SCREEN_PICTURE, 0, (unsigned int) FRAME) == -EFAULT,
          "a picture is in the memory of the program");
    check(screen(99, 0, 0) == -EINVAL, "the kernel knows three requests");

    check(run_child(ask_as_child) == 0, "the kernel refuses the screen to a second process");
    print(STDOUT, "a child touches the screen of its parent\n");
    check(STOP_REASON(run_child(touch_as_child)) != 0, "the kernel has stopped the child");
    check(FRAME[0] == 0x12345678, "the parent still has the screen");

    check(screen(SCREEN_CLOSE, 0, 0) == 0, "the program gives the screen back");
    check(run_child(take_and_end) == 0, "a child takes the screen and ends with it");
    print(STDOUT, "a child takes the screen and writes to the address 0\n");
    check(STOP_REASON(run_child(take_and_fail)) != 0, "the kernel has stopped the child");
    check(screen(SCREEN_OPEN, 0, 0) == (int) FRAME, "the screen is free after both");

    return get_result();
}
