/*
    cp: copies a file. The copy gets the permissions of the file, and it
    takes the place of a file that has its name.
*/

#include "user.h"

#define BUF_SIZE 1024
#define PERMISSIONS 0777 // the bits of a mode that are permissions

int main(int argc, char *argv[])
{
    char buf[BUF_SIZE];

    if (argc != 3) {
        print(STDERR, "usage: cp file copy\n");
        return 1;
    }

    stat_t info;
    int in = open(argv[1], O_RDONLY, 0);
    if (in < 0 || stat(argv[1], &info) != 0) {
        print(STDERR, "cp: cannot read ");
        print(STDERR, argv[1]);
        print(STDERR, "\n");
        return 1;
    }

    int status = 0;
    int out = creat(argv[2], info.mode & PERMISSIONS);
    if (out < 0) {
        status = 1;
    }

    int count;
    while (status == 0 && (count = read(in, buf, BUF_SIZE)) != 0) {
        if (count < 0 || write(out, buf, count) != count) {
            status = 1;
        }
    }

    if (status != 0) {
        print(STDERR, "cp: cannot write ");
        print(STDERR, argv[2]);
        print(STDERR, "\n");
    }

    return status;
}
