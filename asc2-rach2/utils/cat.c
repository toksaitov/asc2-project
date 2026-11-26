/*
    cat: prints files. Without arguments it prints what it reads from its
    input.
*/

#include "user.h"

#define BUF_SIZE 1024

// Copies the rest of an open file to the output. Returns 0 if all of it was copied.
static int copy(int fd)
{
    char buf[BUF_SIZE];

    int count;
    while ((count = read(fd, buf, BUF_SIZE)) > 0) {
        if (write(STDOUT, buf, count) != count) {
            return 1;
        }
    }

    return count;
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        return copy(STDIN) == 0 ? 0 : 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0 || copy(fd) != 0) {
            print(STDERR, "cat: cannot print ");
            print(STDERR, argv[i]);
            print(STDERR, "\n");
            status = 1;
        }
        if (fd >= 0) {
            close(fd);
        }
    }

    return status;
}
