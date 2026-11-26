/*
    touch: sets the time of files to now. A file that is not there is made,
    empty.
*/

#include "user.h"

#define NEW_FILE_MODE 0644

int main(int argc, char *argv[])
{
    if (argc == 1) {
        print(STDERR, "usage: touch file...\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_WRONLY | O_CREAT, NEW_FILE_MODE);
        if (fd < 0 || utimensat(fd, NULL, NULL, 0) != 0) {
            print(STDERR, "touch: cannot touch ");
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
