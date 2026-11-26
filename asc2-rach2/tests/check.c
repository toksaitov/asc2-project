/*
    What the programs that check the kernel share.

    A test program prints one line for every check. The line says what must
    be true, and it starts with `FAILED: ` if it is not. The test suite
    compares the lines with the lines that it expects.
*/

#include "check.h"

#include "user.h"

static int failures;

void check(int is_right, const char *text)
{
    if (!is_right) {
        ++failures;
        print(STDOUT, "FAILED: ");
    }

    print(STDOUT, text);
    print(STDOUT, "\n");
}

int get_result(void)
{
    return failures == 0 ? 0 : 1;
}

void spin(int ticks)
{
    int start = times(NULL);
    while (times(NULL) - start < ticks) {
    }
}
