/*
    echo: prints its arguments with one space between them, and a new line.
*/

#include "user.h"

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            print(STDOUT, " ");
        }
        print(STDOUT, argv[i]);
    }
    print(STDOUT, "\n");

    return 0;
}
