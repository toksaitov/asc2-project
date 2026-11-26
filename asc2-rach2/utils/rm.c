/*
    rm: removes files.
*/

#include "user.h"

int main(int argc, char *argv[])
{
    if (argc == 1) {
        print(STDERR, "usage: rm file...\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        if (unlink(argv[i]) != 0) {
            print(STDERR, "rm: cannot remove ");
            print(STDERR, argv[i]);
            print(STDERR, "\n");
            status = 1;
        }
    }

    return status;
}
