/*
    mv: gives a file a new name. A file that has the new name is removed.
*/

#include "user.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        print(STDERR, "usage: mv name new_name\n");
        return 1;
    }

    if (rename(argv[1], argv[2]) != 0) {
        print(STDERR, "mv: cannot rename ");
        print(STDERR, argv[1]);
        print(STDERR, "\n");
        return 1;
    }

    return 0;
}
